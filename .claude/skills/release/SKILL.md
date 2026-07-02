---
name: release
description: Cut a stable secure-exec release — npm + crates.io in lockstep, plus the manual @secure-exec/core wasm publish. Use whenever the user asks to release secure-exec (a real version, not a preview).
---

# Release secure-exec (stable)

Releases publish the `@secure-exec/*` npm packages AND the `secure-exec-*`
crates at the SAME version. Previews are a different flow (npm-only branch
dist-tag; see CLAUDE.md "Preview-publishing").

1. **Cut the release** from a clean, pushed main checkout:

   ```bash
   just release --patch -y        # or --minor / --major / --version <v>; -rc. versions get the rc tag
   ```

   This bumps versions, commits, pushes, and dispatches `publish.yaml` with the
   version input → release build: npm `@latest`, crates.io, GitHub release assets.

2. **Watch it green**:

   ```bash
   run=$(gh run list -R rivet-dev/secure-exec --workflow=publish.yaml -L1 --json databaseId --jq '.[0].databaseId')
   gh run watch -R rivet-dev/secure-exec "$run" --exit-status
   ```

3. **Manual step — `@secure-exec/core`** (CI-excluded; its tarball vendors the
   wasm commands and CI does not build them):

   ```bash
   make -C registry/native wasm
   cd packages/core && npm publish     # npm, NOT pnpm; same version CI used; prepack fails loud if commands absent
   ```

4. **Registry packages are NOT part of a release** — they version per-package;
   use the `publish-registry` skill.

5. Downstream: an agent-os release passes this version via
   `just release --secure-exec-version <v>` (agent-os `release-agentos` skill).
