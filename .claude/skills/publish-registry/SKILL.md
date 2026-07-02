---
name: publish-registry
description: Publish @agentos-software/* registry packages (per-package semver; dist-tag dev by default, latest only deliberately). Use whenever the user asks to publish or release registry software/agent packages.
---

# Publish registry packages

Registry packages version **independently** (per-package semver in each
`package.json`). Publishing never moves `latest` unless asked. Full lifecycle
reference: `registry/README.md`.

1. **Build** (skip what's already built):

   ```bash
   just registry-native            # native wasm binaries, once per checkout (slow)
   just registry-build [pkg]       # stage bin/ + assemble dist/package
   just registry-status --remote   # local state vs published dist-tags
   ```

2. **Bump the version** in `registry/software/<pkg>/package.json` (or
   `registry/agent/<pkg>/`) and commit it.

3. **Publish**:

   ```bash
   just registry-publish <pkg>            # dist-tag dev (safe default)
   just registry-publish <pkg> latest     # DELIBERATE release — moves latest
   just registry-publish-all [tag]        # every built software package
   ```

Notes:
- secure-exec **previews** (publish.yaml, no version input) automatically
  include all registry packages under the branch dist-tag — no manual step for
  preview consumers.
- agent-os pins per-package: `just agentos-pkgs-update [tag]` /
  `just agentos-pkgs-set-version <pkg> <v>` over there.
