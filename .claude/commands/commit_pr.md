---
description: Run pre-commit hooks to a clean state, commit, push, open a PR, merge it, and clean up the branch.
---

Do everything the `commit` command does, then take the change all the way through a merged PR and back to a clean `master`.

1. Follow the `commit` command's steps 1–5 in full: branch check/creation (`f/`/`b/`/`chore/` naming, never commit on `master`), `python -m pre_commit run --all-files` to a clean state, the commit itself (with the `Co-Authored-By: Claude <noreply@anthropic.com>` trailer), and the push (`git push` / `git push -u origin <branch>`). Never skip hooks, never work around a real hook failure.
2. Open the PR using the `pr-workflow` skill so it follows this repo's established PR conventions (title, body, labels, etc.) rather than reinventing them here.
3. Merge the PR: this repo only allows squash merges (`allow_squash_merge: true`; `allow_merge_commit`/`allow_rebase_merge` are both `false`), so merge with `gh pr merge <number> --squash`. `master` has no required status checks configured, so the merge should go through immediately — if `gh` reports it can't merge (conflicts, an unexpected required check, etc.), stop and report rather than forcing it. Don't pass `--delete-branch`: the repo already has `delete_branch_on_merge` enabled, so GitHub deletes the remote branch itself on merge.
4. Switch back to `master`: `git checkout master`.
5. Sync `master`: `git pull` (should fast-forward cleanly since nothing else is committed directly to `master`).
6. Delete the now-merged local branch: `git branch -D <branch>` — use `-D` (force), not `-d`. Because the merge was a squash, the local branch's commits aren't literally an ancestor of the new `master` commit, so `-d`'s "already merged" safety check will refuse to delete it even though the change is fully in `master`. Confirm first (`git log master -1` matches the change, `git status` clean) before force-deleting.
7. Finish with `git status` and `git branch` to confirm you're on an up-to-date `master` with the feature branch gone.
