/**
 * Example SiteConfig — a standalone site proving @rivet-dev/docs-theme works
 * end-to-end, and exercising the escape hatches (component override + extra css).
 *
 * @type {import('@rivet-dev/docs-theme').SiteConfig}
 */
export const siteConfig = {
	product: "Example",
	repo: "rivet-dev/docs-theme",
	topNav: [
		{ label: "Documentation", href: "/docs", match: "/docs" },
		{ label: "Changelog", href: "https://github.com/rivet-dev/docs-theme/releases" },
	],
	cta: { label: "Get Started", href: "/docs/getting-started" },
	social: { discord: "https://rivet.dev/discord" },

	landing: {
		title: "Example Docs",
		subtitle:
			"A standalone site proving the @rivet-dev/docs-theme package renders correctly outside any monorepo.",
		cards: [
			{ title: "Getting Started", href: "/docs/getting-started", icon: "rocket", description: "Install the theme and ship a docs site." },
			{ title: "Concepts", href: "/docs/guides/concepts", icon: "book", description: "How the theme is put together." },
		],
	},

	// Escape hatches: override a component slot + append an extra stylesheet.
	components: { EditLink: "./src/CustomEditLink.astro" },
	css: ["./src/extra.css"],

	sidebar: [
		{
			label: "General",
			items: [{ slug: "docs", label: "Overview", attrs: { "data-icon": "info" } }],
		},
		{
			label: "Getting Started",
			items: [{ slug: "docs/getting-started", attrs: { "data-icon": "rocket" } }],
		},
		{
			label: "Guides",
			items: [
				{
					// Nested collapsible sub-group ("Concepts >") inside a static
					// top-level group — exercises the non-collapsible/collapsible split.
					label: "Concepts",
					items: [{ slug: "docs/guides/concepts", attrs: { "data-icon": "book" } }],
				},
			],
		},
	],
};

export default siteConfig;
