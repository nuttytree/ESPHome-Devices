#!/usr/bin/env python3
"""Run clang-tidy over this repo's custom C++ components.

This is a slimmed-down port of upstream ESPHome's `script/clang-tidy`, and it uses the
same `.clang-tidy` check list, so the components here are held to the same C++ rules as
ESPHome core. Two things make it more than a plain `clang-tidy components/**/*.cpp`:

* clang cannot consume the xtensa/riscv GCC command line the real build uses, so the
  compiler flags are rebuilt from the build's *idedata* (compiler flags, defines and
  include dirs) with the GCC-only flags dropped and the Arduino/newlib headers that use
  GNU extensions macro-replaced. That flag surgery is copied from upstream.
* the components only compile against a *device's* generated `esphome/core/defines.h`
  (USE_ESP32, USE_ESP8266, which core components are enabled, ...), so the idedata has
  to come from a real build.

Upstream keeps dedicated "tidy" PlatformIO environments to produce that idedata. This
repo has no platformio.ini of its own, so it comes from an actual device build instead
-- which CI already does:

    esphome compile devices/pool.yaml
    python scripts/clang_tidy.py pool

Only the components that device built are checked, with that device's real defines, so
a component that builds for both platforms gets analyzed once per platform (an esp32
build says nothing about the esp8266 one, same as for the compile itself).
"""

import argparse
import concurrent.futures
import json
import os
from pathlib import Path
import re
import subprocess
import sys

REPO_ROOT = Path(__file__).resolve().parent.parent
COMPONENTS_DIR = REPO_ROOT / "components"
DEVICES_DIR = REPO_ROOT / "devices"

IN_ACTIONS = os.environ.get("GITHUB_ACTIONS") == "true"

# GCC-only flags the ESP-IDF/Arduino builds emit that clang rejects outright.
OMIT_FLAGS = (
    "-free",
    "-fipa-pta",
    "-fstrict-volatile-bitfields",
    "-mlongcalls",
    "-mtext-section-literals",
    "-mdisable-hardware-atomics",
    "-mfix-esp32-psram-cache-issue",
    "-mfix-esp32-psram-cache-strategy=memw",
    "-fno-tree-switch-conversion",
    "-freorder-blocks",
    "-fno-jump-tables",
    "-fno-shrink-wrap",
    "-mno-target-align",
)


def toolchain_triplet(cxx_path):
    """Target triplet from the compiler filename, e.g. `xtensa-esp32s3-elf`."""
    name = Path(cxx_path).name
    if name.lower().endswith(".exe"):
        name = name[: -len(".exe")]
    return re.sub(r"-(g|c|clang)\+\+$", "", name)


def path_regex(path):
    """Regex matching `path` with either separator.

    clang-tidy matches --header-filter against the path as the platform spells it, so a
    filter built with one separator silently matches nothing on the other -- and a
    header filter that matches nothing hides every finding in our headers.
    """
    return "[\\\\/]".join(
        re.escape(part) for part in str(path).replace("\\", "/").split("/")
    )


def clang_options(idedata, src_dir):
    """Translate a build's idedata into a clang command line.

    Ported from upstream `script/clang-tidy`; the comments explaining *why* each hack is
    needed are kept, since they encode a lot of hard-won detail about these toolchains.
    """
    cmd = []
    triplet = toolchain_triplet(idedata["cxx_path"])

    if triplet.startswith("xtensa-"):
        # clang has an Xtensa frontend, but only a generic core -- the esp32 IDF
        # toolchain headers (xtruntime, xtensa/config) need the GCC core config
        # (XCHAL_*) it doesn't ship, so we still compile in 32-bit x86 mode and
        # just pretend to be Xtensa. Undefine the host x86 arch macros -m32 sets,
        # and define the xtensa endianness macro newlib's machine/ieeefp.h then
        # needs in their place.
        cmd += ["-m32", "-U__i386__", "-U__x86_64__"]
        cmd += ["-D__XTENSA__", "-D__XTENSA_EL__", "-D_LIBC"]
    else:
        # RISC-V has a real clang backend, so compile for the actual triplet.
        cmd.append(f"--target={triplet}")

    # The build passes flags (e.g. -fno-plt) that clang accepts for some targets but
    # not others, plus a redundant -nostdinc++ below; an unused one is not a code
    # quality signal, and .clang-tidy makes every warning an error.
    cmd.append("-Qunused-arguments")

    # The toolchain headers need clang to claim GCC compatibility: newlib's _ansi.h and
    # sys/cdefs.h key their __GNUC_PREREQ ladders off __GNUC__, and with it undefined
    # they fall back to `throw()` exception specifications (removed in C++17, so a hard
    # error), a locally redefined __alignof, and no __GCC_ATOMIC_* macros for
    # libstdc++'s atomic_base.h -- a dozen errors in headers the real build compiles
    # fine. On Linux, where CI runs, clang defines __GNUC__ 4.2.1 by default and this
    # flag changes nothing; on Windows it defaults to an MSVC target that defines none
    # of it, so state the same version explicitly rather than diverge per host.
    cmd.append("-fgnuc-version=4.2.1")

    cmd += [
        # disable built-in include directories from the host
        "-nostdinc",
        "-nostdinc++",
        # allow to condition code on the presence of clang-tidy
        "-DCLANG_TIDY",
        # (esp-idf) Fix __once_callable in some libstdc++ headers
        "-D_GLIBCXX_HAVE_TLS",
        # suppress warning about attribute cannot be applied to type
        # https://github.com/esp8266/Arduino/pull/8258
        "-Ddeprecated(x)=",
        # replace pgmspace.h, as it uses GNU extensions clang doesn't support
        # https://github.com/earlephilhower/newlib-xtensa/pull/18
        "-D_PGMSPACE_H_",
        "-Dpgm_read_byte(s)=(*(const uint8_t *)(s))",
        "-Dpgm_read_byte_near(s)=(*(const uint8_t *)(s))",
        "-Dpgm_read_word(s)=(*(const uint16_t *)(s))",
        "-Dpgm_read_dword(s)=(*(const uint32_t *)(s))",
        "-Dpgm_read_ptr(s)=(*(const void *const *)(s))",
        "-DPROGMEM=",
        "-DPGM_P=const char *",
        "-DPSTR(s)=(s)",
        # this next one is also needed with upstream pgmspace.h
        # suppress warning about identifier naming in expansion of this macro
        "-DPSTRN(s, n)=(s)",
    ]

    # Espressif's RISC-V GCC -march adds vendor extensions (xesploop, xespv) clang
    # doesn't know; clang rejects the whole arch string otherwise.
    def strip_esp_march(flag):
        if flag.startswith("-march=") and triplet.startswith("riscv"):
            return re.sub(r"_xesp\w+", "", flag)
        return flag

    # A single idedata flag can carry several newline-separated flags: ESP-IDF puts them
    # in a GCC response file, and on Windows ESPHome tokenizes that with the Win32 argv
    # parser, which doesn't treat a newline as a separator. Flatten first, or the
    # GCC-only flags ride into clang inside a token that never matches OMIT_FLAGS.
    build_flags = [
        line for flag in idedata["cxx_flags"] for line in flag.splitlines() if line
    ]

    # Copy the build's compiler flags, dropping the ones clang doesn't understand and
    # -Werror* (clang-tidy enforces .clang-tidy's WarningsAsErrors, and a build -Werror
    # would bypass the -clang-diagnostic-* suppressions there).
    cmd += [
        strip_esp_march(flag)
        for flag in build_flags
        if flag not in OMIT_FLAGS
        and not flag.startswith("-Werror")
        and not flag.startswith("-mtune=esp")
    ]
    if not any(flag.startswith("-std=") for flag in cmd):
        cmd.append("-std=gnu++20")

    # Analyze with logging fully enabled, as upstream's tidy environments do. These
    # devices build with `log_level: none`, which preprocesses every ESP_LOG* call
    # away -- that hides the code inside them from analysis entirely, and makes
    # variables that exist only to be logged look unused.
    defines = [
        define
        for define in idedata["defines"]
        if not define.startswith("ESPHOME_LOG_LEVEL=")
    ]
    defines.append("ESPHOME_LOG_LEVEL=ESPHOME_LOG_LEVEL_VERY_VERBOSE")
    cmd += [f"-D{define}" for define in defines]

    # Toolchain include directories, as -isystem so their own warnings are suppressed.
    # idedata lists the include dirs of every toolchain for the platform; only use the
    # one actually in use.
    # normpath both sides of the prefix test: idedata keeps the separators the build
    # used, which on Windows are not the ones os.path builds the toolchain dir with.
    toolchain_dir = os.path.normpath(f"{idedata['cxx_path']}/../../")
    for directory in idedata["includes"]["toolchain"]:
        if (
            os.path.normpath(directory).startswith(toolchain_dir)
            and "picolibc" not in directory
        ):
            cmd += ["-isystem", directory]

    # Library/framework include directories, likewise -isystem: warnings from ESPHome
    # core and the SDKs aren't ours to fix. The generated sources root is the exception
    # -- it is added with -I below so that `esphome/...` includes resolve.
    src = str(src_dir)
    for directory in idedata["includes"]["build"]:
        if os.path.normpath(directory) != src:
            cmd += ["-isystem", directory]
    cmd += ["-I", src]

    return cmd


def load_idedata(device_config):
    """Read the compiled build's idedata (`esphome idedata`, as JSON)."""
    proc = subprocess.run(
        ["esphome", "idedata", str(device_config)],
        capture_output=True,
        encoding="utf-8",
        check=False,
    )
    if proc.returncode != 0:
        sys.stderr.write(proc.stdout + proc.stderr)
        raise SystemExit(
            f"esphome idedata failed for {device_config}. Compile it first: "
            f"esphome compile {device_config}"
        )
    # esphome logs to stdout ahead of the JSON document, so decode from the first brace
    # rather than parsing the whole stream.
    start = proc.stdout.find("{")
    if start == -1:
        raise SystemExit(f"No idedata JSON in esphome output for {device_config}")
    return json.JSONDecoder().raw_decode(proc.stdout[start:])[0]


def find_src_dir(idedata):
    """Locate the build's generated sources root (`<build_path>/src`).

    That directory holds the device's generated `esphome/core/defines.h` plus the copy
    of ESPHome core the components include, and it is on the build's own include path,
    so it can be recovered from idedata instead of re-resolving the device's build_path.
    """
    for directory in idedata["includes"]["build"]:
        path = Path(directory)
        if path.name == "src" and (path / "esphome" / "core").is_dir():
            return path
    raise SystemExit(
        "Could not find the build's src directory in idedata -- was the device compiled "
        "with a different ESPHome version?"
    )


def components_in_build(src_dir):
    """The repo's components that this device actually built, in name order."""
    built = src_dir / "esphome" / "components"
    return sorted(
        path.name
        for path in COMPONENTS_DIR.iterdir()
        if path.is_dir() and (built / path.name).is_dir()
    )


def run_tidy(executable, options, header_filter, fix, path):
    invocation = [executable, f"--header-filter={header_filter}"]
    if fix:
        invocation.append("--fix")
    if sys.stdout.isatty():
        invocation.append("--use-color")
    invocation += [str(path), "--"] + options

    proc = subprocess.run(
        invocation, capture_output=True, encoding="utf-8", check=False
    )
    return proc.returncode == 0, proc.stdout + proc.stderr


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "device",
        help="name of the compiled device config in devices/ (e.g. pool) supplying the "
        "build flags",
    )
    parser.add_argument(
        "files",
        nargs="*",
        default=[],
        help="only check files whose path matches one of these regexes",
    )
    parser.add_argument("--fix", action="store_true", help="apply fix-its")
    parser.add_argument(
        "-j",
        "--jobs",
        type=int,
        default=os.cpu_count(),
        help="number of clang-tidy instances to run in parallel",
    )
    parser.add_argument(
        "--executable", default="clang-tidy", help="clang-tidy binary to use"
    )
    args = parser.parse_args()

    device_config = DEVICES_DIR / f"{args.device}.yaml"
    if not device_config.is_file():
        raise SystemExit(f"No such device config: {device_config}")

    idedata = load_idedata(device_config)
    src_dir = find_src_dir(idedata)
    options = clang_options(idedata, src_dir)

    components = components_in_build(src_dir)
    if not components:
        print(f"{args.device} builds none of this repo's components; nothing to check")
        return 0

    files = sorted(
        path
        for component in components
        for path in (COMPONENTS_DIR / component).glob("*.cpp")
    )
    if args.files:
        pattern = re.compile("|".join(args.files))
        files = [path for path in files if pattern.search(path.as_posix())]
    if not files:
        print("No files to check")
        return 0

    # Only our own headers are in scope; ESPHome core and the SDK headers they pull in
    # are reported against upstream, not here.
    header_filter = f"{path_regex(COMPONENTS_DIR)}[\\\\/].*"

    print(
        f"Checking {len(files)} file(s) from {', '.join(components)} "
        f"against the {args.device} build"
    )
    failed = []
    # --fix rewrites headers shared by several translation units, so serialize it.
    jobs = 1 if args.fix else max(1, args.jobs)
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as executor:
        futures = {
            executor.submit(
                run_tidy, args.executable, options, header_filter, args.fix, path
            ): path
            for path in files
        }
        for future in concurrent.futures.as_completed(futures):
            path = futures[future]
            ok, output = future.result()
            if not ok:
                failed.append(path)
            if output.strip():
                # Collapse each file's output into a log group on Actions; the
                # problem matcher still annotates the lines inside it.
                name = path.relative_to(REPO_ROOT).as_posix()
                print(f"::group::{name}" if IN_ACTIONS else f"--- {name}")
                print(output)
                if IN_ACTIONS:
                    print("::endgroup::")

    if failed:
        print("\nclang-tidy failed for:")
        for path in sorted(failed):
            print(f"  {path.relative_to(REPO_ROOT).as_posix()}")
        return 1
    print("clang-tidy passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
