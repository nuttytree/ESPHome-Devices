#!/usr/bin/env python3
"""Tag the current commit with the lowest ESPHome version every device is confirmed running.

Reads devices/.esphome/.device-builder-devices.json (local Device Builder state) and, once
every device yaml in devices/ reports a deployed_version with a deployed_config_hash matching
its expected_config_hash, tags the current commit with the lowest of those versions -- unless
a tag at or above that version already exists. Intended to run as a post-commit hook so HEAD
is the commit being tagged. Never pushes; run `git push --follow-tags` (or push the tag
explicitly) when you're ready to publish it.
"""

import json
import re
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
DEVICES_DIR = REPO_ROOT / "devices"
DEVICES_JSON = DEVICES_DIR / ".esphome" / ".device-builder-devices.json"
EXCLUDED_DEVICE_YAML = {"secrets.yaml"}

VERSION_RE = re.compile(r"^\d+(?:\.\d+)*")


def version_key(version):
    match = VERSION_RE.match(version)
    if not match:
        return None
    return tuple(int(part) for part in match.group().split("."))


def device_yaml_files():
    return sorted(
        p.name
        for p in DEVICES_DIR.glob("*.yaml")
        if p.name not in EXCLUDED_DEVICE_YAML and not p.name.startswith(".")
    )


def git_tag_versions():
    result = subprocess.run(
        ["git", "tag", "--list"],
        cwd=REPO_ROOT,
        capture_output=True,
        text=True,
        check=True,
    )
    versions = {}
    for tag in result.stdout.splitlines():
        tag = tag.strip()
        key = version_key(tag)
        if key:
            versions[key] = tag
    return versions


def main():
    if not DEVICES_JSON.exists():
        print(
            f"tag_esphome_version: {DEVICES_JSON.relative_to(REPO_ROOT)} not found, skipping"
        )
        return 0

    devices = json.loads(DEVICES_JSON.read_text())
    yaml_files = device_yaml_files()
    if not yaml_files:
        print("tag_esphome_version: no device yaml files found, skipping")
        return 0

    floor_key = None
    floor_version = None
    problems = []

    for name in yaml_files:
        entry = devices.get(name)
        if entry is None:
            problems.append(f"{name}: not present in device builder state")
            continue

        deployed = entry.get("deployed_version")
        if not deployed:
            problems.append(f"{name}: no deployed_version recorded")
            continue

        if entry.get("deployed_config_hash") != entry.get("expected_config_hash"):
            problems.append(
                f"{name}: deployed config is stale (not yet flashed with the current config)"
            )
            continue

        key = version_key(deployed)
        if key is None:
            problems.append(f"{name}: could not parse deployed_version '{deployed}'")
            continue

        if floor_key is None or key < floor_key:
            floor_key = key
            floor_version = deployed

    if problems:
        print(
            "tag_esphome_version: skipping, not every device confirms this commit yet:"
        )
        for problem in problems:
            print(f"  - {problem}")
        return 0

    existing = git_tag_versions()
    if any(key >= floor_key for key in existing):
        print(
            f"tag_esphome_version: already have a tag at or above {floor_version}, nothing to do"
        )
        return 0

    message = f"This commit is compatible with ESPHome version {floor_version}"
    subprocess.run(
        ["git", "tag", "-a", floor_version, "-m", message], cwd=REPO_ROOT, check=True
    )
    print(
        f"tag_esphome_version: tagged commit as {floor_version} "
        f"(push with `git push --follow-tags` or `git push origin {floor_version}`)"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
