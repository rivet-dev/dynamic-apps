/**
 * Secure Exec docs configuration for @rivet-dev/docs-theme (the de-Starlighted,
 * rivet-1:1 framework). Maps Secure Exec's product identity, navigation, and
 * pages onto the theme's SiteConfig.
 *
 * `sitemap` is the docs navigation tree: SiteTab[] where each tab carries a
 * sidebar tree (pages + collapsible sections). Routes are /docs/* (file paths
 * under src/content/docs). Top-level sections are non-collapsible labels; only
 * nested page-groups collapse. Page items carry FontAwesome `IconDefinition`s
 * for the sidebar icons.
 *
 * @type {import('@rivet-dev/docs-theme').SiteConfig}
 */
import {
	faCircleInfo,
	faRocket,
	faTerminal,
	faFileCode,
	faCode,
	faBook,
	faRobot,
	faServer,
	faPuzzlePiece,
	faPlay,
	faScroll,
	faBolt,
	faBox,
	faCodeBranch,
	faWrench,
	faShieldHalved,
	faFolder,
	faNetworkWired,
	faGauge,
	faNodeJs,
	faCodeCompare,
	faCubes,
	faLock,
} from "@rivet-gg/icons";

export const siteConfig = {
	product: "Secure Exec",
	productLogo: "/secure-exec-logo-long-ink.svg",
	productHome: "/",
	favicon: "/favicon.svg",
	repo: "rivet-dev/secure-exec",
	editPath: "website/",

	// Local CSS appended after the shared theme stylesheet (so it overrides it).
	css: ["./src/styles/docs-overrides.css"],

	// Docs pages have no top-nav links (Documentation/Changelog live on the
	// marketing page's own Navigation). The header keeps logo + search + CTA.
	topNav: [],
	// Single tab → the theme hides the secondary tab strip (only agentOS, with 2
	// tabs, shows it). Kept here so the docs section context still resolves.
	tabs: [{ label: "Documentation", href: "/docs", match: "/docs" }],
	cta: { label: "Get Started", href: "/docs/quickstart" },
	social: { discord: "https://rivet.dev/discord" },

	analytics: { posthogKey: "phc_6kfTNEAVw7rn1LA51cO3D69FefbKupSWFaM7OUgEpEo" },

	// Hosted Typesense docs search (same cluster as rivet). The search-only key
	// is safe to ship client-side; indexing uses the populate key (see scripts).
	search: {
		typesense: {
			host: "3lsug6t152oxcjndp-1.a1.typesense.net",
			searchApiKey: "3R49clF2Np3eoBoqd6PtYyEIqkA5nNVZ",
			collectionName: "secureexec-docs",
		},
	},

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

	sitemap: [
		{
			title: "Documentation",
			href: "/docs",
			sidebar: [
				{ title: "Introduction", href: "/docs", icon: faCircleInfo },
				{
					title: "Getting Started",
					pages: [
						{ title: "Quickstart", href: "/docs/quickstart", icon: faRocket },
						{ title: "Crash Course", href: "/docs/crash-course", icon: faTerminal },
						{
							title: "SDKs",
							icon: faCode,
							collapsible: true,
							pages: [
								{ title: "TypeScript", href: "/docs/sdks/typescript", icon: faFileCode },
								{ title: "Rust", href: "/docs/sdks/rust", icon: faCode },
								{ title: "TypeScript API Reference", href: "/api", external: true, target: "_blank", icon: faBook },
							],
						},
					],
				},
				{
					title: "Use Cases",
					pages: [
						{ title: "AI Agent Code Exec", href: "/docs/use-cases/ai-agent-code-exec", icon: faRobot },
						{ title: "Code Mode", href: "/docs/use-cases/code-mode", icon: faCode },
						{ title: "Dev Servers", href: "/docs/use-cases/dev-servers", icon: faServer },
						{ title: "Plugin Systems", href: "/docs/use-cases/plugin-systems", icon: faPuzzlePiece },
					],
				},
				{
					title: "Node.js Runtime",
					pages: [
						{ title: "Executing Code", href: "/docs/features/executing-code", icon: faPlay },
						{ title: "Output Capture", href: "/docs/features/output-capture", icon: faScroll },
						{ title: "Resident Runner", href: "/docs/features/resident-runner", icon: faBolt },
						{ title: "TypeScript", href: "/docs/features/typescript", icon: faFileCode },
						{ title: "NPM & Module Loading", href: "/docs/features/module-loading", icon: faBox },
						{ title: "Runtime & Platform", href: "/docs/features/runtime-platform", icon: faCode },
						{ title: "Child Processes", href: "/docs/features/child-processes", icon: faCodeBranch },
						{ title: "Bindings", href: "/docs/features/bindings", icon: faWrench },
					],
				},
				{
					title: "Virtual Machine",
					pages: [
						{ title: "Permissions", href: "/docs/features/permissions", icon: faShieldHalved },
						{ title: "Filesystem", href: "/docs/features/filesystem", icon: faFolder },
						{ title: "Networking", href: "/docs/features/networking", icon: faNetworkWired },
						{ title: "Resource Limits", href: "/docs/features/resource-limits", icon: faGauge },
					],
				},
				{
					title: "Reference",
					pages: [
						{ title: "Node.js Compatibility", href: "/docs/nodejs-compatibility", icon: faNodeJs },
						{ title: "Benchmarks", href: "/docs/benchmarks", icon: faGauge },
						{
							title: "Comparison",
							collapsible: true,
							pages: [
								{ title: "vs Sandbox", href: "/docs/comparison/sandbox", icon: faCodeCompare },
								{ title: "vs Cloudflare Workers", href: "/docs/comparison/cloudflare-workers", icon: faCodeCompare },
								{ title: "vs QuickJS", href: "/docs/comparison/quickjs", icon: faCodeCompare },
								{ title: "vs isolated-vm", href: "/docs/comparison/isolated-vm", icon: faCodeCompare },
							],
						},
						{
							title: "Advanced",
							collapsible: true,
							pages: [
								{ title: "Architecture", href: "/docs/architecture", icon: faCubes },
								{ title: "Security Model", href: "/docs/security-model", icon: faLock },
							],
						},
					],
				},
			],
		},
	],
};

export default siteConfig;
