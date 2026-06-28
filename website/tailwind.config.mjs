import docsThemePreset from "@rivet-dev/docs-theme/tailwind-preset";

/** @type {import('tailwindcss').Config} */
export default {
  // The docs-theme preset brings rivet's components base (semantic tokens like
  // `foreground`/`card`/`muted`) that the theme's CSS @applies.
  presets: [docsThemePreset],
  content: [
    "./src/**/*.{astro,html,js,jsx,md,mdx,svelte,ts,tsx,vue}",
    "./node_modules/@rivet-dev/docs-theme/src/**/*.{astro,ts,tsx,jsx,js,mdx,md}",
    "./node_modules/@rivet-gg/components/src/**/*.{ts,tsx}",
  ],
  theme: {
    extend: {
      colors: {
        accent: "#ff4f00",
        background: "#09090b",
        "bg-secondary": "#0f0f11",
        "bg-tertiary": "#0c0c0e",
        "text-primary": "#ffffff",
        "text-secondary": "#a1a1aa",
        "text-tertiary": "#71717a",
        border: "rgba(255, 255, 255, 0.10)",
        "code-keyword": "#c084fc",
        "code-function": "#60a5fa",
        "code-string": "#4ade80",
        "code-comment": "#71717a",
      },
      fontFamily: {
        sans: ["IBM Plex Sans", "Segoe UI", "system-ui", "sans-serif"],
        heading: ["IBM Plex Sans", "Segoe UI", "system-ui", "sans-serif"],
        // Match rivet + the theme's code blocks: JetBrains Mono for all mono text.
        mono: ['"JetBrains Mono"', "SFMono-Regular", "monospace"],
      },
      animation: {
        "fade-in-up": "fade-in-up 0.8s ease-out forwards",
        "hero-line": "hero-line 1s cubic-bezier(0.19, 1, 0.22, 1) forwards",
        "hero-p": "hero-p 0.8s ease-out 0.6s forwards",
        "hero-cta": "hero-p 0.8s ease-out 0.8s forwards",
        "hero-visual": "hero-p 0.8s ease-out 1s forwards",
        "pulse-slow": "pulse-slow 3s cubic-bezier(0.4, 0, 0.6, 1) infinite",
      },
      keyframes: {
        "fade-in-up": {
          from: { opacity: "0", transform: "translateY(24px)" },
          to: { opacity: "1", transform: "translateY(0)" },
        },
        "hero-line": {
          "0%": { opacity: "0", transform: "translateY(100%) skewY(6deg)" },
          "100%": { opacity: "1", transform: "translateY(0) skewY(0deg)" },
        },
        "hero-p": {
          from: { opacity: "0", transform: "translateY(20px)" },
          to: { opacity: "1", transform: "translateY(0)" },
        },
        "pulse-slow": {
          "50%": { opacity: ".5" },
        },
      },
      spacing: {
        header: "var(--header-height, 3.5rem)",
      },
      borderRadius: {
        "4xl": "2rem",
      },
    },
  },
  plugins: [],
};
