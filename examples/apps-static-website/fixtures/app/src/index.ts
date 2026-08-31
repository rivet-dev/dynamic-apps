import { readFile } from "node:fs/promises";
import { Hono } from "hono";

const contentTypes: Record<string, string> = {
	".html": "text/html; charset=utf-8",
	".css": "text/css; charset=utf-8",
	".js": "text/javascript; charset=utf-8",
};

const app = new Hono();

// Serve every file under public/. "/" maps to public/index.html. The build
// bundles this entrypoint next to public/, so paths resolve from the bundle.
app.get("/*", async (c) => {
	const path = c.req.path.endsWith("/")
		? `${c.req.path}index.html`
		: c.req.path;
	const type = contentTypes[path.slice(path.lastIndexOf("."))];
	if (!type || path.includes("..")) return c.notFound();
	try {
		const file = await readFile(new URL(`./public${path}`, import.meta.url));
		return c.body(file, 200, { "content-type": type });
	} catch {
		return c.notFound();
	}
});

export default app;
