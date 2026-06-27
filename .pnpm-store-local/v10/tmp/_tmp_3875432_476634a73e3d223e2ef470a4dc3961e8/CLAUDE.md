# @rivet-dev/docs-theme

Shared Starlight docs theme/framework for Rivet OSS projects (secure-exec and
agent-os-docs are the first consumers). One fixed Rivet "porcelain" brand;
consumers pass a `SiteConfig` and never configure Starlight directly. Distributed
by `github:` URL pinned to a tag (no npm publishing). `example/` is a standalone
site that proves the package end-to-end — keep it building (`pnpm --dir example
build`) when changing the package.

## How to update

- **Brand/chrome** (palette, fonts, header, sidebar, code blocks): edit this
  package. `src/styles/theme.css` (porcelain tokens + `@font-face`),
  `src/expressive-code.mjs` (code blocks), `src/components/*.astro` (chrome),
  `src/icons.mjs` (shared FA icon catalog). Re-test via `pnpm --dir website build`.
- **A consumer's identity/nav/content**: that belongs in the consumer's
  `docs.config.mjs` (`SiteConfig`), never hardcoded here.
- After changing the `SiteConfig` shape, update `src/site-config.d.ts`,
  `src/virtual.d.ts`, and `README.md` together — the config is a shared contract.

## Invariants

- **Ship source only** — `.astro`/`.css`/`.mjs`/`.woff2`. No build/`prepare`/
  `postinstall` step; Astro/Vite compile at the consumer's build time.
- **Never import `@astrojs/starlight` here.** Starlight ships TS source that Node
  can't strip under `node_modules`; the consumer imports `starlight` and passes
  the factory into `docsTheme(starlight, config)`.
- **No React peer dep.** Interactive bits (e.g. `GitHubStars.astro`) are plain
  Astro + inline scripts.
- **No hardcoded routes/projects.** Sidebar icons come from `entry.attrs['data-icon']`
  (fed by a manual sidebar item's `attrs` or a page's `sidebar.attrs`), resolved
  against `src/icons.mjs`. Header/landing data comes from the virtual module
  `virtual:rivet-docs/config`. Zero `secure-exec` references belong in this package.
- Fonts are bundled via relative `url()` in `theme.css` (Vite hashes + emits
  them) — do not reference an absolute `/fonts/...` path.
- PostHog host is fixed (`ph.rivet.gg`); only the project key is configurable.
