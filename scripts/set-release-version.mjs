import { readFile, writeFile } from "node:fs/promises";

const version = process.argv[2];
if (!version) throw new Error("usage: set-release-version.mjs <version>");

async function update(path, transform) {
	const value = JSON.parse(await readFile(path, "utf8"));
	transform(value);
	await writeFile(path, `${JSON.stringify(value, null, "\t")}\n`);
}

await update("packages/dynamic-apps-builder/package.json", (value) => {
	value.version = version;
});
await update("packages/dynamic-apps/package.json", (value) => {
	value.version = version;
	value.dependencies["@rivet-dev/dynamic-apps-builder"] = version;
});
