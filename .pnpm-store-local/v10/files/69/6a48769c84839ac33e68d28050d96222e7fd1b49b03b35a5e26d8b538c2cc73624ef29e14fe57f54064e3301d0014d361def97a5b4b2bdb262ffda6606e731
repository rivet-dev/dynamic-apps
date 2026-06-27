import type { AstroIntegration } from "astro";
import type { StarlightUserConfig } from "@astrojs/starlight/types";

export interface NavItem {
	/** Link text. */
	label: string;
	/** Destination URL (absolute path or external). */
	href: string;
	/** Pathname prefix that marks this item as the current page (e.g. `/docs`). */
	match?: string;
}

export interface CallToAction {
	label: string;
	href: string;
}

export interface LandingCard {
	title: string;
	href: string;
	/** Icon key from the shared catalog (e.g. `rocket`, `terminal`, `book`). */
	icon: string;
	description: string;
	badge?: string;
}

export interface Landing {
	title: string;
	subtitle: string;
	cards: LandingCard[];
}

export interface SiteConfig {
	/** Product name — document title and the header wordmark fallback text. */
	product: string;
	/** Optional wordmark image URL shown beside the Rivet mark instead of text. */
	productLogo?: string;
	/** Header product link target. Defaults to `/`. */
	productHome?: string;
	/** Favicon path. Defaults to `/favicon.svg`. */
	favicon?: string;
	/** `owner/name` — derives the GitHub social link, stars widget, and edit links. */
	repo?: string;
	/** Default branch for edit links. Defaults to `main`. */
	branch?: string;
	/** Sub-path prefix for edit links (e.g. `website/`). Defaults to empty. */
	editPath?: string;
	/** Primary header navigation links. */
	topNav?: NavItem[];
	/** Header call-to-action button. */
	cta?: CallToAction;
	/** Social links. `github` is derived from `repo` when omitted. */
	social?: { discord?: string; github?: string };
	/** Sidebar tree, passed through to Starlight. */
	sidebar: StarlightUserConfig["sidebar"];
	/** Docs landing hero + card grid (rendered by the shared DocsLanding). */
	landing?: Landing;
	/** Analytics. PostHog host is fixed to the shared Rivet instance. */
	analytics?: { posthogKey?: string };

	// --- Escape hatches for diverse OSS consumers ---
	/** Extra Starlight config, deep-merged onto the generated config. */
	starlight?: Partial<StarlightUserConfig>;
	/** Extra CSS module specifiers, appended after the theme stylesheet. */
	css?: string[];
	/** Override any Starlight component slot with your own. */
	components?: StarlightUserConfig["components"];
}

type StarlightFactory = (config: any) => AstroIntegration;

/**
 * Build the docs-theme integrations for an Astro project. Spread into
 * `integrations` in `astro.config.mjs`. Pass the `starlight` integration
 * factory (imported in the consumer's config) as the first argument; Starlight
 * ships TypeScript source, so importing it must stay on the consumer side.
 *
 *   import starlight from "@astrojs/starlight";
 *   import { docsTheme } from "@rivet-dev/docs-theme";
 *   integrations: [react(), tailwind(), ...docsTheme(starlight, siteConfig)]
 */
export function docsTheme(starlight: StarlightFactory, config: SiteConfig): AstroIntegration[];

/** The companion integration alone (exposes the SiteConfig virtual module). */
export function docsThemeIntegration(config: SiteConfig): AstroIntegration;

export default docsTheme;
