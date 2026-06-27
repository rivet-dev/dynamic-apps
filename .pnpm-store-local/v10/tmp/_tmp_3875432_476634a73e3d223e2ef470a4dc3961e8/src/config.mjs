/**
 * Maps a project's `SiteConfig` onto a Starlight user config. Consumers never
 * touch Starlight directly — `docsTheme()` (see index.mjs) calls this and feeds
 * the result to `starlight()`. The fixed Rivet-porcelain brand (theme.css,
 * Expressive Code, component overrides) is wired here; only content-shaped
 * fields (title, repo, nav, sidebar) come from the project.
 */
import { fileURLToPath } from "node:url";
import { expressiveCode } from "./expressive-code.mjs";

const asset = (rel) => fileURLToPath(new URL(rel, import.meta.url));

// PostHog host is fixed to the shared Rivet instance; only the project key
// varies. Mirrors the snippet the Rivet docs ship.
function posthogHead(key) {
	const snippet = `
!function(t,e){var o,n,p,r;e.__SV||(window.posthog=e,e._i=[],e.init=function(i,s,a){function g(t,e){var o=e.split(".");2==o.length&&(t=t[o[0]],e=o[1]),t[e]=function(){t.push([e].concat(Array.prototype.slice.call(arguments,0)))}}(p=t.createElement("script")).type="text/javascript",p.async=!0,p.src=s.api_host+"/static/array.js",(r=t.getElementsByTagName("script")[0]).parentNode.insertBefore(p,r);var u=e;for(void 0!==a?u=e[a]=[]:a="posthog",u.people=u.people||[],u.toString=function(t){var e="posthog";return"posthog"!==a&&(e+="."+a),t||(e+=" (stub)"),e},u.people.toString=function(){return u.toString(1)+".people (stub)"},o="capture identify alias people.set people.set_once set_config register register_once unregister opt_out_capturing has_opted_out_capturing opt_in_capturing reset isFeatureEnabled onFeatureFlags getFeatureFlag getFeatureFlagPayload reloadFeatureFlags group updateEarlyAccessFeatureEnrollment getEarlyAccessFeatures getActiveMatchingSurveys getSurveys getNextSurveyStep onSessionId".split(" "),n=0;n<o.length;n++)g(u,o[n]);e._i.push([i,s,a])},e.__SV=1)}(document,window.posthog||[]);
posthog.init('${key}',{api_host:'https://ph.rivet.gg',session_recording:{maskAllInputs:false}});
`;
	return { tag: "script", content: snippet };
}

export function buildStarlightConfig(config) {
	const repo = config.repo;
	const branch = config.branch ?? "main";
	const editPath = config.editPath ?? "";

	const components = {
		Header: asset("./components/Header.astro"),
		ThemeSelect: asset("./components/ThemeSelect.astro"),
		PageTitle: asset("./components/PageTitle.astro"),
		Sidebar: asset("./components/Sidebar.astro"),
		EditLink: asset("./components/EditLink.astro"),
		...(config.components ?? {}),
	};

	const head = [];
	if (config.analytics?.posthogKey) head.push(posthogHead(config.analytics.posthogKey));
	if (config.starlight?.head) head.push(...config.starlight.head);

	const social =
		config.starlight?.social ??
		(repo
			? [
					{
						icon: "github",
						label: "GitHub",
						href: `https://github.com/${repo}`,
					},
				]
			: undefined);

	const { head: _h, social: _s, customCss: _c, components: _cc, ...starlightRest } =
		config.starlight ?? {};

	return {
		title: config.product,
		favicon: config.favicon ?? "/favicon.svg",
		lastUpdated: true,
		customCss: [asset("./styles/theme.css"), ...(config.css ?? [])],
		components,
		expressiveCode,
		...(repo
			? {
					editLink: {
						baseUrl: `https://github.com/${repo}/edit/${branch}/${editPath}`,
					},
				}
			: {}),
		...(social ? { social } : {}),
		...(head.length ? { head } : {}),
		sidebar: config.sidebar,
		...starlightRest,
	};
}
