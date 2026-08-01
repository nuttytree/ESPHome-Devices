# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

This is an [ESPHome](https://esphome.io) configuration repository: YAML device configs plus custom external components (Python codegen + C++) for a collection of home-automation devices (pool controller, HVAC zone control, energy monitors, smart plugs, etc.) that integrate with Home Assistant. It is not an application with a build/test pipeline in the traditional sense — the "build" is ESPHome compiling a device's YAML into firmware, and the "tests" are lint/format checks plus ESPHome's own YAML config validation.

Compiling and flashing devices normally happens through the ESPHome Dashboard/Device Builder, which mounts this repo's `devices/` directory as its config directory (its state files are described below). A local `esphome` CLI is also installed on this Windows machine via `pipx` (isolated from system Python) — useful for `esphome config devices/<file>.yaml` to validate a device's full resolved config without going through Device Builder; see the "ESPHome CLI" section below for details, gotchas, and update instructions. Full local compiles are possible too, but need one-time machine setup — see "Compiling locally".

## Commands

Linting/formatting is enforced via pre-commit (see `.pre-commit-config.yaml`) and run in CI on every push/PR (`.github/workflows/lint.yml`):

```
pre-commit run --all-files       # run every hook against the whole repo
pre-commit run --files <path>    # run against specific file(s)
```

Individual hooks, if you need to invoke a tool directly instead of via pre-commit:
- `ruff check --fix` / `ruff format` — lint + format for all Python (components).
- `flake8` — additional docstring/style checks, scoped to `components/**/*.py` only (`.flake8`).
- `yamllint` — all YAML files except `.clang-format`/`.clang-tidy` (`.yamllint`: 2-space indent, no line-length limit, `document-start` disabled).
- `clang-format` — C/C++ files in `components/**` (`.clang-format`).

### ESPHome CLI

Installed via `pipx install esphome`, which creates its own isolated virtualenv rather than touching system Python packages.

- **Must be installed under Python 3.12+**, not this machine's default Python 3.11 (`C:\Program Files\Python311`). ESPHome 2026.7.0+ requires Python `>=3.12,<3.15`; installing under 3.11 doesn't error, it silently resolves to the newest 3.11-compatible release instead (e.g. 2026.6.5 when 2026.7.2 was current), which can surface as spurious "Platform not found" errors for components added in newer releases. Use the `py -3.12` launcher (or the full interpreter path) when installing/upgrading:
  ```
  pipx install --python "C:\Users\<user>\AppData\Local\Programs\Python\Python312\python.exe" esphome
  ```
- The `esphome.exe` shim lands in the pipx bin dir (`%USERPROFILE%\.local\bin`), which `pipx ensurepath` adds to the persistent user PATH — new terminals/sessions should resolve `esphome` directly. A shell that was already running before the PATH update won't see it until restarted; fall back to the full venv path (`...\pipx\venvs\esphome\Scripts\esphome.exe` — check `pipx list` for the exact location, it can be nested as `pipx\pipx\venvs\...` depending on how pipx itself was installed) if `esphome` isn't found.
- To check/upgrade: `pipx upgrade esphome`, or reinstall with the `--python` flag above if it's drifted onto the wrong interpreter.
- `esphome config devices/<file>.yaml` validates and fully resolves a device config (schema + substitutions + packages) without needing a compile toolchain — much faster than a full `esphome compile` and doesn't require platformio.

### Compiling locally (ESP-IDF toolchain)

`esphome config` needs none of this; `esphome compile` needs all of it. Compiling normally happens inside the Device Builder/Docker container, and the `devices/.esphome/build/<device>` directories in this repo were produced *there* (paths rooted at `/config/...`), so a Windows build can't reuse them — it fails with `The current CMakeCache.txt directory ... is different than the directory ... where CMakeCache.txt was created`. A host compile therefore always starts from a fresh build directory, and needs the two setup steps below.

**1. Put the ESP-IDF toolchain on a short path.** Windows' default `MAX_PATH` is 260 characters and the IDF toolchain nests ~245 characters below its tools directory, so the default location (`%LOCALAPPDATA%\esphome\Cache\idf`) overflows it. The failure is cryptic — `fatal error: bits/c++config.h: No such file or directory`, or `cannot execute 'as'` — though ESPHome does print a warning naming the problem first. Either fix works:

- Enable long path support (needs elevation, then a reboot):
  ```
  Set-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Control\FileSystem' LongPathsEnabled 1
  ```
- Or point ESPHome at a short prefix, which it maps straight to `IDF_TOOLS_PATH`. This is what the current machine does (`LongPathsEnabled` is `0` there, and the prefix is `C:\eh`):
  ```
  [Environment]::SetEnvironmentVariable('ESPHOME_ESP_IDF_PREFIX','C:\eh','User')
  ```
  On a fresh machine ESPHome downloads roughly 6 GB into that prefix on the first compile. To migrate an existing install instead of re-downloading, move `%LOCALAPPDATA%\esphome\Cache\idf` to the new prefix, then delete `tools\`, `penvs\` and `idf-env.json` inside it — those re-extract from the `dist\` archive cache that moves along with them.

**2. Build to a local drive, not the UNC share.** This repo lives on `\\nas\docker\esphome\config`, and `devices/packages/device_base.yaml` sets `build_path: ./build/${device_id}`, which resolves (relative to the device yaml) to `devices/build/${device_id}` on that share. `cmd.exe` cannot use a UNC path as a working directory, so ninja's *link* step runs from `C:\Windows` and fails with `ld.exe: cannot open output file ...: Permission denied`. The compile steps succeed first, which makes this read as a linker bug rather than a path problem. Override `build_path` to a local path (e.g. `C:/eh/bld/<name>`) when compiling from Windows. Source files can stay on the share — gcc handles UNC paths fine; only the working directory is the problem.

**3. Don't run any other `esphome` command while a compile is in progress.** Even `esphome config` rewrites state under `devices/.esphome/`, and because that directory is shared with the build, cmake sees a changed input and re-runs configure mid-build. The initial configure succeeds off the resolved dependency lock, but a *re-run* re-resolves component manifests and dies on `devices/.esphome/pio_components/idf/*/esphome/noise-c/idf_component.yml`, which contains `override_path: /config/.esphome/...` — a container path, written by the Docker ESPHome where `devices/` is mounted at `/config`. Windows resolves it against the UNC root as `\\nas\docker\config\...`, which doesn't exist, and the build dies with `The "override_path" field ... does not point to a directory`. That cache is gitignored and regenerable, so leave it alone rather than "fixing" it — the container needs those paths. Just validate before you build, then stay off the config directory until it finishes.

Similarly, don't kill a compile during its managed-component download: a partial `lvgl/lvgl` left without its `.component_hash` fails every later build with `File .component_hash or CHECKSUMS.json ... does not exist or cannot be parsed`. Delete the build directory and start over.

To compile-check a component without disturbing the container's build caches, copy the device YAML to a throwaway `device_id` with a local `build_path`, compile that copy, then delete both the scratch YAML and its build directory. Keep the copy inside `devices/` so its relative `!include ./packages/...` and `path: ../components` still resolve. A full IDF build takes roughly 10–15 minutes. Note that `esphome config` validates schema and lambdas' *syntax* but does not compile lambda C++ — only a real compile catches errors inside a `lambda:` block.

## Git workflow

The `no-commit-to-branch` pre-commit hook blocks commits directly on `master`. All work happens on a feature branch and lands via PR (see recent history: `Merge pull request #77`, etc.). Use the `pr-workflow` skill when asked to open a PR for this repo.

## Architecture

**`devices/*.yaml`** are one-per-physical-device ESPHome configs (`pool.yaml`, `hvac.yaml`, `water-heater.yaml`, `coffee-maker.yaml`, ...). Each declares `substitutions` (at minimum `device_id`, `device_name`, `api_key`/`pwd` pulled from `!secret`), pulls in a base package, and then adds device-specific hardware config (GPIO pins, i2c/uart buses, sensors, etc.) inline.

`devices/` is the directory mounted as `/config` in the ESPHome Device Builder container — the dashboard only scans the top level of its config dir, which is why the device yamls, their `packages/`, and `secrets.yaml` all live together there while everything repo-level (`components/`, `docs/`, `scripts/`, lint config) stays at the repo root. `components/` is bind-mounted alongside it so that `path: ../components` resolves identically inside the container and from a host `esphome` CLI run.

**`devices/packages/`** holds shared YAML fragments included by devices via the `packages:` key, layered like this:
- `device_base.yaml` — the common core every device gets: wifi, api, ota, logger, status/uptime/wifi sensors, restart button, version/wifi-info text sensors, time.
- `device_base_esp32.yaml` / `device_base_esp8266.yaml` — platform block (`esp32:`/`esp8266:`) plus an include of `device_base.yaml`. Devices include one of these, not `device_base.yaml` directly.
- `econet_base.yaml` — extends the esp32 base for Econet-driven HVAC boards, internalizing a couple of entities via `!extend`.
- `wifi/wifi_dhcp.yaml` vs `wifi/wifi.yaml` — DHCP vs static-IP wifi variants.
- `devices/secrets.yaml` (untracked, gitignored) holds actual secret values; every device/package references secrets with `!secret key_name`. `devices/packages/secrets.yaml` and the various per-device `secrets.yaml` files just include that one.

The `!extend` tag is used throughout to modify entities defined by an included package/component (e.g. marking a base sensor `internal: true`, or attaching an `on_value`/`on_time` trigger to `wifi_rssi`/`time_source` in `pool.yaml`) instead of redefining them.

**`components/`** holds custom external ESPHome components, each in its own directory with:
- `__init__.py` (+ platform files like `sensor.py`, `climate.py`, `light.py`, `water_heater.py`) — config schema (`esphome.config_validation`) and C++ codegen (`esphome.codegen`), following standard ESPHome component conventions.
- `.cpp`/`.h` — the runtime implementation.
- `README.md` — authoritative, hand-maintained documentation of that component's YAML config schema and behavior. **Keep a component's README in sync whenever its config schema or behavior changes** — these are the primary docs (there's no separate docs site) and are detailed enough to read before modifying a component's config surface.

Components are consumed either from disk (`external_components: - source: {type: local, path: ../components}` — relative to the device yaml in `devices/`, used by devices in this repo) or by external users via `source: github://nuttytree/ESPHome-Devices`.

**Version compatibility tags**: git tags like `2026.7.0` mark commits confirmed compatible with that ESPHome version. `scripts/tag_esphome_version.py` runs as a `post-commit` pre-commit hook (see `default_install_hook_types` / the `tag-esphome-version` local hook in `.pre-commit-config.yaml`) and automates this: it reads the local, gitignored `devices/.esphome/.device-builder-devices.json` (Device Builder state), and if every device yaml in `devices/` has a `deployed_version` whose `deployed_config_hash` matches `expected_config_hash`, it tags HEAD with the lowest such version — unless a tag at or above it already exists. It never pushes the tag; push explicitly (`git push origin <tag>`) or rely on `git config push.followTags true` so a normal `git push` carries it along. Because it's a post-commit hook it can't block a commit — if devices aren't fully represented/up to date yet, it just prints why it skipped.

**Sibling `dev_esphome/` directory** (outside this repo, listed as an additional working directory) is a full checkout of upstream ESPHome core, wired in purely for editor IntelliSense (`python.analysis.extraPaths` / `C_Cpp.default.includePath` in `.vscode/settings.json`, `PYTHONPATH` in `.env`). It is not part of this project — don't make changes there as part of tasks against this repo.

**Sibling `esphome.io/` directory** (outside this repo, also listed as an additional working directory) is a clone of the [esphome/esphome.io](https://github.com/esphome/esphome.io) documentation site (the Astro-based source for the public esphome.io docs), with a `fork` remote at `nuttytree/esphome.io` alongside `origin`. Like `dev_esphome/`, it's a separate project checked out here for reference/IntelliSense — not part of this repo, so don't make changes there as part of tasks against this repo unless explicitly asked to work on the docs site itself.

**Device Builder state files** in `devices/` (`.device-builder.json`, `.device-builder-preferences.json`, `.device-builder-peer-link-key.bin`) are machine-generated local state for the ESPHome dashboard/device-builder tool (device MAC addresses, firmware job history, UI prefs) — not meant to be hand-edited.

## Conventions

- YAML: 2-space indent, double-quoted strings, LF line endings (`.gitattributes` forces LF for `*.yaml`/`*.yml` even on this Windows checkout).
- Python (`components/**/*.py`): standard ESPHome component style; flake8 ignores docstring rules (`D1xx`) since ESPHome components aren't expected to have docstrings, and line length is capped at 120.
