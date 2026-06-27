---
title: Concepts
description: How the theme is put together.
---

The theme is an Astro integration that wraps Starlight. You pass a `SiteConfig`;
the package owns everything visual. Sidebar icons attach via each item's
`attrs.data-icon`, resolved against the shared icon catalog.

Top-level sidebar groups are static section headers; nested sub-groups (like the
"Concepts" group this page lives in) are collapsible.
