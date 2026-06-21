/**
 * Secure Exec docs configuration — the only non-content surface consumed by
 * @rivet-dev/docs-theme. Everything visual (theme, header chrome, sidebar
 * icons, code blocks) lives in the package; this file maps Secure Exec's
 * product identity, navigation, and pages onto it.
 *
 * Sidebar leaves carry their icon via `attrs['data-icon']` (resolved against
 * the theme's shared icon catalog), so the package never hardcodes routes.
 */

/** @type {import('@rivet-dev/docs-theme').SiteConfig} */
export const siteConfig = {
	product: "Secure Exec",
	productLogo: "/secure-exec-logo-long-ink.svg",
	productHome: "/",
	favicon: "/favicon.svg",
	repo: "rivet-dev/secure-exec",
	editPath: "website/",

	// Local CSS appended after the shared theme stylesheet (so it overrides it).
	css: ["./src/styles/docs-overrides.css"],

	topNav: [],
	cta: { label: "Get Started", href: "/docs/quickstart" },
	social: { discord: "https://rivet.dev/discord" },

	analytics: { posthogKey: "phc_6kfTNEAVw7rn1LA51cO3D69FefbKupSWFaM7OUgEpEo" },

	landing: {
		title: "Documentation",
		subtitle:
			"A lightweight library for secure Node.js execution. No containers, no VMs — just npm-compatible sandboxing out of the box.",
		cards: [
			{ title: "Quickstart", href: "/docs/quickstart", icon: "rocket", description: "Install and run your first sandboxed execution in a few minutes." },
			{ title: "Crash Course", href: "/docs/crash-course", icon: "terminal", description: "A fast tour of the secure-exec SDK: run code, capture output, and the core concepts." },
			{ title: "Executing Code", href: "/docs/features/executing-code", icon: "layers", description: "Run code and TypeScript, capture output, and load npm modules in the sandbox." },
		],
	},

	sidebar: [
		{ slug: "docs", label: "Introduction", attrs: { "data-icon": "info" } },
		{
			label: "Getting Started",
			items: [
				{ slug: "docs/quickstart", attrs: { "data-icon": "rocket" } },
				{ slug: "docs/crash-course", label: "Crash Course", attrs: { "data-icon": "terminal" } },
				{
					label: "SDKs",
					collapsed: true,
					items: [
						{ slug: "docs/sdks/typescript", attrs: { "data-icon": "fileCode" } },
						{ slug: "docs/sdks/rust", attrs: { "data-icon": "code" } },
						{ label: "TypeScript API Reference", link: "/api", attrs: { "data-icon": "book", target: "_blank" } },
					],
				},
			],
		},
		{
			label: "Use Cases",
			items: [
				{ slug: "docs/use-cases/ai-agent-code-exec", attrs: { "data-icon": "bot" } },
				{ slug: "docs/use-cases/code-mode", attrs: { "data-icon": "code" } },
				{ slug: "docs/use-cases/dev-servers", attrs: { "data-icon": "server" } },
				{ slug: "docs/use-cases/plugin-systems", attrs: { "data-icon": "puzzle" } },
			],
		},
		{
			label: "Node.js Runtime",
			items: [
				{ slug: "docs/features/executing-code", attrs: { "data-icon": "play" } },
				{ slug: "docs/features/output-capture", attrs: { "data-icon": "scroll" } },
				{ slug: "docs/features/resident-runner", attrs: { "data-icon": "zap" } },
				{ slug: "docs/features/typescript", attrs: { "data-icon": "fileCode" } },
				{ slug: "docs/features/module-loading", label: "NPM & Module Loading", attrs: { "data-icon": "package" } },
				{ slug: "docs/features/runtime-platform", label: "Runtime & Platform", attrs: { "data-icon": "code" } },
				{ slug: "docs/features/child-processes", attrs: { "data-icon": "split" } },
				{ slug: "docs/features/bindings", attrs: { "data-icon": "wrench" } },
			],
		},
		{
			label: "Virtual Machine",
			items: [
				{ slug: "docs/features/permissions", attrs: { "data-icon": "shield" } },
				{ slug: "docs/features/filesystem", attrs: { "data-icon": "folder" } },
				{ slug: "docs/features/networking", attrs: { "data-icon": "network" } },
				{ slug: "docs/features/resource-limits", attrs: { "data-icon": "gauge" } },
			],
		},
		{
			label: "Reference",
			items: [
				{ slug: "docs/nodejs-compatibility", attrs: { "data-icon": "nodejs" } },
				{ slug: "docs/benchmarks", attrs: { "data-icon": "gauge" } },
				{
					label: "Comparison",
					items: [
						{ slug: "docs/comparison/sandbox", attrs: { "data-icon": "gitCompare" } },
						{ slug: "docs/comparison/cloudflare-workers", attrs: { "data-icon": "gitCompare" } },
						{ slug: "docs/comparison/quickjs", label: "vs QuickJS", attrs: { "data-icon": "gitCompare" } },
						{ slug: "docs/comparison/isolated-vm", label: "vs isolated-vm", attrs: { "data-icon": "gitCompare" } },
					],
				},
				{
					label: "Advanced",
					items: [
						{ slug: "docs/architecture", attrs: { "data-icon": "blocks" } },
						{ slug: "docs/security-model", attrs: { "data-icon": "lock" } },
					],
				},
			],
		},
	],
};

export default siteConfig;
