# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

This is an [ESPHome](https://esphome.io) configuration repository: YAML device configs plus custom external components (Python codegen + C++) for a collection of home-automation devices (pool controller, HVAC zone control, energy monitors, smart plugs, etc.) that integrate with Home Assistant. It is not an application with a build/test pipeline in the traditional sense — the "build" is ESPHome compiling a device's YAML into firmware, and the "tests" are lint/format checks plus ESPHome's own YAML config validation.

There is no local `esphome` CLI installed in this environment. Compiling and flashing devices happens through the ESPHome Dashboard/Device Builder that owns this config directory (its state files are described below) — don't assume `esphome compile`/`esphome config` are runnable from a shell here.

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

## Git workflow

The `no-commit-to-branch` pre-commit hook blocks commits directly on `master`. All work happens on a feature branch and lands via PR (see recent history: `Merge pull request #77`, etc.). Use the `pr-workflow` skill when asked to open a PR for this repo.

## Architecture

**Root-level `*.yaml` files** are one-per-physical-device ESPHome configs (`pool.yaml`, `hvac.yaml`, `water-heater.yaml`, `coffee-maker.yaml`, ...). Each declares `substitutions` (at minimum `device_id`, `device_name`, `api_key`/`pwd` pulled from `!secret`), pulls in a base package, and then adds device-specific hardware config (GPIO pins, i2c/uart buses, sensors, etc.) inline.

**`packages/`** holds shared YAML fragments included by devices via the `packages:` key, layered like this:
- `device_base.yaml` — the common core every device gets: wifi, api, ota, logger, status/uptime/wifi sensors, restart button, version/wifi-info text sensors, time.
- `device_base_esp32.yaml` / `device_base_esp8266.yaml` — platform block (`esp32:`/`esp8266:`) plus an include of `device_base.yaml`. Devices include one of these, not `device_base.yaml` directly.
- `econet_base.yaml` — extends the esp32 base for Econet-driven HVAC boards, internalizing a couple of entities via `!extend`.
- `wifi/wifi_dhcp.yaml` vs `wifi/wifi.yaml` — DHCP vs static-IP wifi variants.
- Root `secrets.yaml` (untracked, gitignored) holds actual secret values; every device/package references secrets with `!secret key_name`. `packages/secrets.yaml` and the various per-device `secrets.yaml` files just include the root one.

The `!extend` tag is used throughout to modify entities defined by an included package/component (e.g. marking a base sensor `internal: true`, or attaching an `on_value`/`on_time` trigger to `wifi_rssi`/`time_source` in `pool.yaml`) instead of redefining them.

**`components/`** holds custom external ESPHome components, each in its own directory with:
- `__init__.py` (+ platform files like `sensor.py`, `climate.py`, `light.py`, `water_heater.py`) — config schema (`esphome.config_validation`) and C++ codegen (`esphome.codegen`), following standard ESPHome component conventions.
- `.cpp`/`.h` — the runtime implementation.
- `README.md` — authoritative, hand-maintained documentation of that component's YAML config schema and behavior. **Keep a component's README in sync whenever its config schema or behavior changes** — these are the primary docs (there's no separate docs site) and are detailed enough to read before modifying a component's config surface.

Components are consumed either from disk (`external_components: - source: {type: local, path: ./components}`, used by devices in this repo) or by external users via `source: github://nuttytree/ESPHome-Devices`.

**Version compatibility tags**: git tags like `2026.7.0` mark commits confirmed compatible with that ESPHome version. `scripts/tag_esphome_version.py` runs as a `post-commit` pre-commit hook (see `default_install_hook_types` / the `tag-esphome-version` local hook in `.pre-commit-config.yaml`) and automates this: it reads the local, gitignored `.esphome/.device-builder-devices.json` (Device Builder state), and if every root-level device yaml has a `deployed_version` whose `deployed_config_hash` matches `expected_config_hash`, it tags HEAD with the lowest such version — unless a tag at or above it already exists. It never pushes the tag; push explicitly (`git push origin <tag>`) or rely on `git config push.followTags true` so a normal `git push` carries it along. Because it's a post-commit hook it can't block a commit — if devices aren't fully represented/up to date yet, it just prints why it skipped.

**Sibling `dev_esphome/` directory** (outside this repo, listed as an additional working directory) is a full checkout of upstream ESPHome core, wired in purely for editor IntelliSense (`python.analysis.extraPaths` / `C_Cpp.default.includePath` in `.vscode/settings.json`, `PYTHONPATH` in `.env`). It is not part of this project — don't make changes there as part of tasks against this repo.

**Device Builder state files** at the repo root (`.device-builder.json`, `.device-builder-preferences.json`, `.device-builder-peer-link-key.bin`) are machine-generated local state for the ESPHome dashboard/device-builder tool (device MAC addresses, firmware job history, UI prefs) — not meant to be hand-edited.

## Conventions

- YAML: 2-space indent, double-quoted strings, LF line endings (`.gitattributes` forces LF for `*.yaml`/`*.yml` even on this Windows checkout).
- Python (`components/**/*.py`): standard ESPHome component style; flake8 ignores docstring rules (`D1xx`) since ESPHome components aren't expected to have docstrings, and line length is capped at 120.
