/**
 * @rivet-dev/docs-theme — a shared Starlight docs framework for Rivet OSS
 * projects. Consumers add `docsTheme(starlight, siteConfig)` to their Astro
 * `integrations` and supply only content (markdown) plus a landing page.
 * Everything else — the porcelain brand, header chrome, sidebar icons,
 * code-block styling — is owned here.
 *
 * `starlight` is passed in by the consumer (imported in their Vite-transformed
 * astro.config) rather than imported here: Starlight ships TypeScript source,
 * which Node's native loader — used for node_modules packages during config
 * load — cannot strip. Keeping the import on the consumer side sidesteps that
 * while still hiding all Starlight configuration behind this package.
 *
 * `docsTheme()` returns a flat list of integrations: Starlight (configured from
 * the SiteConfig) plus a tiny companion integration that exposes the project's
 * config to the component overrides through a Vite virtual module.
 */
import { buildStarlightConfig } from "./config.mjs";

const VIRTUAL_ID = "virtual:rivet-docs/config";
const RESOLVED_ID = "\0" + VIRTUAL_ID;

// Only content-shaped, JSON-serializable fields reach the browser-side
// component overrides. Escape hatches (starlight/css/components) stay on the
// build side and never get serialized.
function clientConfig(config) {
	return {
		product: config.product,
		productLogo: config.productLogo ?? null,
		productHome: config.productHome ?? "/",
		repo: config.repo ?? null,
		branch: config.branch ?? "main",
		topNav: config.topNav ?? null,
		cta: config.cta ?? null,
		social: config.social ?? {},
		landing: config.landing ?? null,
	};
}

/** The companion integration: exposes the SiteConfig via a Vite virtual module. */
export function docsThemeIntegration(config) {
	const serialized = JSON.stringify(clientConfig(config));
	return {
		name: "@rivet-dev/docs-theme",
		hooks: {
			"astro:config:setup": ({ updateConfig }) => {
				updateConfig({
					vite: {
						plugins: [
							{
								name: "rivet-docs-config",
								resolveId(id) {
									if (id === VIRTUAL_ID) return RESOLVED_ID;
									return null;
								},
								load(id) {
									if (id === RESOLVED_ID) return `export default ${serialized};`;
									return null;
								},
							},
						],
					},
				});
			},
		},
	};
}

/**
 * Build the docs-theme integrations. Spread into `integrations` in
 * `astro.config.mjs`. Pass the `starlight` integration factory (imported in the
 * consumer's config) as the first argument.
 *
 *   import starlight from "@astrojs/starlight";
 *   import { docsTheme } from "@rivet-dev/docs-theme";
 *   integrations: [react(), tailwind(), ...docsTheme(starlight, siteConfig)]
 */
export function docsTheme(starlight, config) {
	return [starlight(buildStarlightConfig(config)), docsThemeIntegration(config)];
}

export default docsTheme;
