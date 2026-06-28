/**
 * Compatibility shim for `@astrojs/starlight/components`.
 *
 * The secure-exec docs CONTENT (src/content/docs/**) was authored against
 * Starlight and still imports `Aside`, `LinkCard`, `Tabs`, `TabItem`, `Steps`,
 * and `CardGrid` from `@astrojs/starlight/components`. The de-Starlighted
 * @rivet-dev/docs-theme ships equivalent MDX primitives under different names
 * and prop shapes. astro.config.mjs aliases the Starlight import path to this
 * file so the existing content builds unchanged.
 *
 * NOTE: this is the build FOUNDATION. Phase 2 rewrites the doc content to import
 * the theme's components directly; once that lands, this shim and its alias can
 * be removed.
 */
import {
	Tabs as ThemeTabs,
	Tab as ThemeTab,
	Steps as ThemeSteps,
	Card as ThemeCard,
	CardGroup as ThemeCardGroup,
	Note,
	Tip,
	Warning,
} from "@rivet-dev/docs-theme/components/mdx.jsx";

export const Tabs = ThemeTabs;

// Starlight's <TabItem label="..."> -> theme's <Tab title="...">
export const TabItem = ({ label, title, ...props }) => (
	<ThemeTab title={label ?? title} {...props} />
);

export const Steps = ThemeSteps;

// Starlight's <CardGrid> -> theme's <CardGroup>
export const CardGrid = ThemeCardGroup;

// Starlight's <LinkCard title href description /> -> theme's <Card>
export const LinkCard = ({ title, href, description, target, ...props }) => (
	<ThemeCard title={title} href={href} target={target} {...props}>
		{description}
	</ThemeCard>
);

// Starlight's <Card title icon> -> theme's <Card>
export const Card = ThemeCard;

// Starlight's <Aside type="note|tip|caution|danger" title>...</Aside>
// -> theme callouts (Note / Tip / Warning).
export const Aside = ({ type = "note", title, children }) => {
	const Variant = type === "tip" ? Tip : type === "caution" || type === "danger" ? Warning : Note;
	return (
		<Variant>
			{title ? <strong>{title}</strong> : null}
			{children}
		</Variant>
	);
};
