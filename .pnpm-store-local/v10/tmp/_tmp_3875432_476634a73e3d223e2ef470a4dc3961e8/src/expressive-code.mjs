/**
 * Expressive Code configuration for the Rivet docs theme.
 *
 * Dark code blocks on the porcelain field, matching the Rivet docs: near-black
 * fill, rounded-xl frame, cream hairline, JetBrains Mono, orange active tab.
 * This is part of the fixed brand — it is not configurable per project.
 */
export const expressiveCode = {
	themes: ["github-dark-default"],
	styleOverrides: {
		borderRadius: "0.75rem",
		borderColor: "rgba(244, 241, 231, 0.1)",
		codeFontFamily:
			'"JetBrains Mono", ui-monospace, SFMono-Regular, Menlo, monospace',
		codeBackground: "#0a0a0a",
		frames: {
			editorTabBarBackground: "#111110",
			editorTabBarBorderBottomColor: "rgba(244, 241, 231, 0.1)",
			editorActiveTabBackground: "#0a0a0a",
			editorActiveTabIndicatorBottomColor: "#cb5a33",
			terminalTitlebarBackground: "#111110",
			terminalBackground: "#0a0a0a",
			frameBoxShadowCssValue: "none",
		},
	},
};
