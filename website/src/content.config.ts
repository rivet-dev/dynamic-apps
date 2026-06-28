import { defineCollection, z } from "astro:content";
import { glob } from "astro/loaders";

// De-Starlighted: a plain glob over the docs tree. Routes are derived from each
// entry id by src/pages/[...slug].astro (e.g. id "docs/quickstart" -> /docs/quickstart).
export const collections = {
	docs: defineCollection({
		loader: glob({ pattern: "**/*.{md,mdx}", base: "./src/content/docs" }),
		schema: z
			.object({
				title: z.string(),
				description: z.string().optional(),
			})
			.passthrough(),
	}),
};
