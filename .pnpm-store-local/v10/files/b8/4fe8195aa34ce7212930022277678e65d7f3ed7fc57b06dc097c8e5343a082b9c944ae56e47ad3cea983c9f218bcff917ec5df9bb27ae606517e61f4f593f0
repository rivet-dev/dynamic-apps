import { defineConfig } from "astro/config";
import starlight from "@astrojs/starlight";
import { docsTheme } from "@rivet-dev/docs-theme";
import { siteConfig } from "./docs.config.mjs";

// https://astro.build/config
export default defineConfig({
	site: "https://example.com",
	integrations: [...docsTheme(starlight, siteConfig)],
});
