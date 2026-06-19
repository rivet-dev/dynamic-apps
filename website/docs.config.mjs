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

	topNav: [
		{ label: "Documentation", href: "/docs", match: "/docs" },
		{ label: "Changelog", href: "https://github.com/rivet-dev/secure-exec/releases" },
	],
	cta: { label: "Get Started", href: "/docs/quickstart" },
	social: { discord: "https://rivet.dev/discord" },

	analytics: { posthogKey: "phc_6kfTNEAVw7rn1LA51cO3D69FefbKupSWFaM7OUgEpEo" },

	landing: {
		title: "Documentation",
		subtitle:
			"Secure Exec is a fully virtualized runtime for executing untrusted code — a POSIX-like VM with a virtual filesystem, process table, sockets, and managed language runtimes, with zero host escapes.",
		cards: [
			{ title: "Quickstart", href: "/docs/quickstart", icon: "rocket", description: "Install and run your first sandboxed execution in a few minutes." },
			{ title: "SDK Overview", href: "/docs/sdk-overview", icon: "terminal", description: "The programmatic API for driving the runtime from Node or the browser." },
			{ title: "Features", href: "/docs/features/typescript", icon: "layers", description: "TypeScript, permissions, filesystem, networking, child processes, and more." },
			{ title: "System Drivers", href: "/docs/system-drivers/overview", icon: "cpu", description: "Run on Node or in the browser through one kernel-owned syscall surface." },
			{ title: "Benchmarks", href: "/docs/benchmarks", icon: "gauge", description: "Performance and cost characteristics versus other approaches." },
			{ title: "API Reference", href: "/docs/api-reference", icon: "book", description: "Full reference for the runtime API." },
		],
	},

	sidebar: [
		{
			label: "General",
			items: [
				{ slug: "docs", label: "Overview", attrs: { "data-icon": "info" } },
			],
		},
		{
			label: "Getting Started",
			items: [
				{ slug: "docs/quickstart", attrs: { "data-icon": "rocket" } },
				{ slug: "docs/sdk-overview", attrs: { "data-icon": "terminal" } },
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
			label: "System Drivers",
			items: [
				{ slug: "docs/system-drivers/overview", attrs: { "data-icon": "layers" } },
				{ slug: "docs/system-drivers/node", attrs: { "data-icon": "hexagon" } },
				{ slug: "docs/system-drivers/browser", attrs: { "data-icon": "globe" } },
			],
		},
		{
			label: "Features",
			items: [
				{ slug: "docs/features/typescript", attrs: { "data-icon": "fileCode" } },
				{ slug: "docs/features/permissions", attrs: { "data-icon": "shield" } },
				{ slug: "docs/features/filesystem", attrs: { "data-icon": "folder" } },
				{ slug: "docs/features/virtual-filesystem", attrs: { "data-icon": "folderTree" } },
				{ slug: "docs/features/networking", attrs: { "data-icon": "network" } },
				{ slug: "docs/features/module-loading", attrs: { "data-icon": "package" } },
				{ slug: "docs/features/runtime-platform", attrs: { "data-icon": "code" } },
				{ slug: "docs/features/output-capture", attrs: { "data-icon": "scroll" } },
				{ slug: "docs/features/resource-limits", attrs: { "data-icon": "gauge" } },
				{ slug: "docs/features/child-processes", attrs: { "data-icon": "split" } },
				{ slug: "docs/process-isolation", attrs: { "data-icon": "box" } },
			],
		},
		{
			label: "Reference",
			items: [
				{ slug: "docs/api-reference", attrs: { "data-icon": "book" } },
				{ slug: "docs/nodejs-compatibility", attrs: { "data-icon": "check" } },
				{ slug: "docs/benchmarks", attrs: { "data-icon": "gauge" } },
				{
					label: "Comparison",
					items: [
						{ slug: "docs/comparison/sandbox", attrs: { "data-icon": "gitCompare" } },
						{ slug: "docs/comparison/cloudflare-workers", attrs: { "data-icon": "gitCompare" } },
					],
				},
				{
					label: "Advanced",
					items: [
						{ slug: "docs/cost-evaluation", attrs: { "data-icon": "dollar" } },
						{ slug: "docs/architecture", attrs: { "data-icon": "blocks" } },
						{ slug: "docs/security-model", attrs: { "data-icon": "lock" } },
					],
				},
			],
		},
	],
};

export default siteConfig;
