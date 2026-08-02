---
name: pr-workflow
description: Create a pull request in this ESPHome config repo (nuttytree/ESPHome-Devices). Use when opening a PR, submitting changes, or preparing contributions from this working directory. This repo's conventions differ from upstream esphome/esphome.
allowed-tools: Read, Bash, Glob, Grep
---

# PR Workflow (nuttytree/ESPHome-Devices)

This is the PR workflow for **this repo**: a personal ESPHome device-config and
custom-component repo, default branch `master`.

> **There is a second, unrelated `pr-workflow` skill in the sibling `dev_esphome/`
> checkout.** That one describes contributing to *upstream ESPHome core*: base on
> `upstream/dev`, fill in `.github/PULL_REQUEST_TEMPLATE.md`, prefix the title with
> `[component]`, `gh pr create --repo esphome/esphome --base dev`. **All of that is
> wrong here.** It only appears as an available skill because `dev_esphome/` is
> configured as an additional working directory for IntelliSense. `dev_esphome/`
> and `esphome.io/` are read-only reference checkouts of other people's projects —
> never push to them or open PRs against them.

## Related commands

- `/commit` — branch, run hooks clean, commit, push. Use it for the pre-PR steps.
- `/commit_pr` — the whole cycle: commit, push, PR, **merge**, and clean up.

Use this skill when you want a PR *opened* but not merged. If the goal is to land
a change end to end, use `/commit_pr` instead.

## 1. Branch and commit

Follow `/commit` in full. Never commit on `master` — the `no-commit-to-branch`
hook blocks it. Branch naming, per this repo's history:

| prefix | use | example |
|---|---|---|
| `f/` | feature or change | `f/pump-restart-resume` |
| `b/` | bug fix | `b/pump-anomaly-detection` |
| `chore/` | maintenance, tooling, docs | `chore/align-lint-with-upstream` |

## 2. Open the PR

Against **this repo only**, base `master`:

```bash
gh pr create --base master --title "<title>" --body-file -
```

This repo has no PR template and uses no labels. Match recent merged PRs
(`gh pr list --state merged --limit 5`, then `gh pr view <n> --json body`):

- **Title** — sentence-case description of the change, no bracket or component
  prefix. E.g. "Distrust stale current readings; measure runtime from flow".
- **Body** — prose statement of the problem and why it mattered, then a
  `Changes:` bullet list of what was actually done. Reference related PRs or
  issues as `#<n>`. Call out anything that silently changes behaviour on upgrade,
  such as persisted state being discarded.
- **Verification** — say how it was checked: `esphome config <device>.yaml`, or an
  `esphome compile` if C++ changed.

## 3. Wait for CI

`master` requires **all seven checks** to pass before a PR can merge:

```
pre-commit        esphome config
compile pool      compile water-heater    compile hvac
compile pool-lights                       compile master-bed
```

`enforce_admins` is on, so this applies to the repo owner too. Budget ~5 minutes:
the five compiles run in parallel, esp32 builds take ~4 min, esp8266 ~2 min.

```bash
gh pr checks <number>
```

Two CI facts worth knowing when a check looks wrong:

- **Compile jobs only run on `pull_request` and pushes to `master`.** Pushing a
  branch with no PR open runs nothing at all. This is deliberate — see the comment
  on the `on:` trigger in `.github/workflows/ci.yml`.
- **`devices/secrets.yaml` is gitignored**, so CI generates a placeholder one via
  `scripts/generate_ci_secrets.py` before validating or compiling. A new `!secret`
  reference is picked up automatically; no CI change needed.

Some lint rules only fail on Linux. `ruff`'s `EXE001` (shebang on a
non-executable file) is skipped on Windows, so a local `pre-commit run
--all-files` can pass while CI fails. Fix with
`git update-index --chmod=+x <file>` rather than dropping the shebang.

## 4. Merging (if asked)

Squash only — `allow_merge_commit` and `allow_rebase_merge` are both `false`:

```bash
gh pr merge <number> --squash --delete-branch
```

- The repo has `delete_branch_on_merge` enabled, so the remote branch goes either
  way. `--delete-branch` additionally switches back to `master`, pulls, and
  deletes the *local* branch — doing steps that would otherwise be manual.
- Default squash body is `COMMIT_MESSAGES`, which concatenates every commit on the
  branch. For a multi-commit branch, pass `--subject` and `--body` explicitly so
  the squashed commit reads as one coherent message in this repo's style.
- Deleting the local branch by hand needs `git branch -D`, not `-d`: after a
  squash the branch's commits aren't ancestors of the new `master` commit, so
  `-d`'s merged-check refuses.

If `gh` reports it can't merge — conflicts, a failing required check — stop and
report rather than forcing it.

### If a required check wedges a PR

A required context that never reports leaves the PR unmergeable. To regain the
admin bypass without disturbing anything else:

```bash
gh api -X DELETE repos/:owner/:repo/branches/master/protection/enforce_admins
```

Narrower `DELETE` endpoints also exist for `required_status_checks` and for the
whole `protection` object. Branch protection never blocks editing the protection
rule itself, and repo-owner admin can't be lost.
