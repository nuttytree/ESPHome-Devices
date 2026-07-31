---
description: Run pre-commit hooks to a clean state, commit, push, open a PR, merge it, and clean up the branch.
---

Do everything the `commit` command does, then take the change all the way through a merged PR and back to a clean `master`.

Every step below operates on **this repo only** (`nuttytree/ESPHome-Devices`, default branch `master`). Never push to, or open a PR against, `esphome/esphome` or any other upstream — the sibling `dev_esphome/` and `esphome.io/` working directories are reference checkouts of other people's projects, not targets for this workflow.

1. Follow the `commit` command's steps 1–5 in full: branch check/creation (`f/`/`b/`/`chore/` naming, never commit on `master`), `python -m pre_commit run --all-files` to a clean state, the commit itself (with the `Co-Authored-By: Claude <noreply@anthropic.com>` trailer), and the push (`git push` / `git push -u origin <branch>`). Never skip hooks, never work around a real hook failure.
2. Open the PR **against this repo only** — `nuttytree/ESPHome-Devices`, base branch `master`:

   ```
   gh pr create --base master --title "<title>" --body-file -
   ```

   > **Do not use the `pr-workflow` skill here, and never target `esphome/esphome`.** That skill lives in the sibling `dev_esphome/` checkout (upstream ESPHome core, wired in only for IntelliSense — see CLAUDE.md) and describes how to contribute *to upstream*: it says to base on `upstream/dev`, fill in ESPHome's `.github/PULL_REQUEST_TEMPLATE.md`, prefix the title with `[component]`, and run `gh pr create --repo esphome/esphome --base dev`. All of that is wrong for this repo. It surfaces as an available skill purely because `dev_esphome/` is an additional working directory. Ignore it; the conventions below are this repo's.

   This repo has no PR template and uses no labels. Match the style of recent merged PRs (`gh pr list --state merged --limit 5`, then `gh pr view <n> --json body`):
   - **Title**: sentence-case description of the change, no bracket/component prefix (e.g. "Fix pump anomaly detection: startup window, latching, drift, variance").
   - **Body**: a prose statement of the problem and why it mattered, then a `Changes:` bullet list of what was actually done. Reference related PRs/issues with `#<n>` where relevant, and call out anything that silently changes behaviour on upgrade (e.g. persisted state being discarded).
   - Note how the change was verified (`esphome config <device>.yaml`, or an `esphome compile` if the C++ changed).
3. Merge the PR: this repo only allows squash merges (`allow_squash_merge: true`; `allow_merge_commit`/`allow_rebase_merge` are both `false`), so merge with `gh pr merge <number> --squash`. `master` has no required status checks configured, so the merge should go through immediately — if `gh` reports it can't merge (conflicts, an unexpected required check, etc.), stop and report rather than forcing it. Don't pass `--delete-branch`: the repo already has `delete_branch_on_merge` enabled, so GitHub deletes the remote branch itself on merge.
4. Switch back to `master`: `git checkout master`.
5. Sync `master`: `git pull` (should fast-forward cleanly since nothing else is committed directly to `master`).
6. Delete the now-merged local branch: `git branch -D <branch>` — use `-D` (force), not `-d`. Because the merge was a squash, the local branch's commits aren't literally an ancestor of the new `master` commit, so `-d`'s "already merged" safety check will refuse to delete it even though the change is fully in `master`. Confirm first (`git log master -1` matches the change, `git status` clean) before force-deleting.
7. Finish with `git status` and `git branch` to confirm you're on an up-to-date `master` with the feature branch gone.
