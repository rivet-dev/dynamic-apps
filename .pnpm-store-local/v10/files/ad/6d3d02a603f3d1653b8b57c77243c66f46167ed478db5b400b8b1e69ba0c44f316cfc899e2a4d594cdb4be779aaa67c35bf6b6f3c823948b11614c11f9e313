/**
 * Type for the Vite virtual module that exposes the project's `SiteConfig` to
 * the component overrides. Provided by the companion integration in index.mjs.
 */
declare module "virtual:rivet-docs/config" {
	import type { CallToAction, Landing, NavItem } from "./site-config";

	const config: {
		product: string;
		productLogo: string | null;
		productHome: string;
		repo: string | null;
		branch: string;
		topNav: NavItem[] | null;
		cta: CallToAction | null;
		social: { discord?: string; github?: string };
		landing: Landing | null;
	};
	export default config;
}
