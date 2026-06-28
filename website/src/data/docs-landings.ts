import {
	faRocket,
	faTerminal,
	faLayerGroup,
} from "@rivet-gg/icons";
import type { DocsLandingData } from "@rivet-dev/docs-theme/components/docs/DocsLanding";

/**
 * Section-overview landings (the icon-grid element, like rivet.dev's docs
 * landings). Keyed by route; [...slug].astro renders <DocsLanding> for a
 * matching path instead of the prose article.
 *
 * Built from secure-exec's `siteConfig.landing` ({ title, subtitle, cards }),
 * mapping each card's icon string onto a FontAwesome IconDefinition.
 */
export const docsLandings: Record<string, DocsLandingData> = {
	"/docs": {
		title: "Documentation",
		subtitle:
			"A lightweight library for secure Node.js execution. No containers, no VMs — just npm-compatible sandboxing out of the box.",
		sections: [
			{
				title: "Get Started",
				items: [
					{ title: "Quickstart", href: "/docs/quickstart", icon: faRocket, description: "Install and run your first sandboxed execution in a few minutes." },
					{ title: "Crash Course", href: "/docs/crash-course", icon: faTerminal, description: "A fast tour of the secure-exec SDK: run code, capture output, and the core concepts." },
					{ title: "Executing Code", href: "/docs/features/executing-code", icon: faLayerGroup, description: "Run code and TypeScript, capture output, and load npm modules in the sandbox." },
				],
			},
		],
	},
};
