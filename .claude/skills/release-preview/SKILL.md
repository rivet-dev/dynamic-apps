---
name: release-preview
description: Cut a secure-exec release-preview — npm-only branch-dist-tag publish (registry packages included), no crates.io. Use when the user asks for a preview / release-preview of secure-exec, or to hand a build to a downstream.
---

# Release-preview secure-exec

A preview publishes the `@secure-exec/*` packages AND the `@agentos-software/*`
registry packages to npm under the sanitized branch dist-tag, versioned
`0.0.0-<branch>.<sha>`, from a fast debug build. No crates.io publish (it has
no preview track — crate changes reach downstreams via their clone-at-sha
builds), no git tag, no release assets.

1. **Push the branch** you want previewed (jj colocated; use
   `jj --config snapshot.max-new-file-size=16777216 ...` if large assets
   complain).

2. **Dispatch + watch**:

   ```bash
   just release-preview <branch>
   run=$(gh run list -R rivet-dev/secure-exec --workflow=publish.yaml -L1 --json databaseId --jq '.[0].databaseId')
   gh run watch -R rivet-dev/secure-exec "$run" --exit-status
   ```

3. **Consume**: `npm install @secure-exec/core@<sanitized-branch>` (same tag for
   the registry packages). agent-os does NOT normally consume these directly —
   it pins a sha in `.github/refs/secure-exec` and its own release-preview
   auto-cuts the matching secure-exec preview (branch `agentos-dep-<sha7>`).

Notes:
- Release-preview is for previews ONLY; never cut a release with it — releases
  go through the `release` skill.
- On failure: `gh run view <run> --log-failed`, fix, re-dispatch, re-watch.
