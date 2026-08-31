import { execFile } from "node:child_process";
import { appendFile } from "node:fs/promises";
import { promisify } from "node:util";

const execFileAsync = promisify(execFile);
const args = Object.fromEntries(
	process.argv.slice(2).map((argument) => {
		const [key, ...value] = argument.replace(/^--/, "").split("=");
		return [key, value.join("=")];
	}),
);

let version = args.version;
if (version === "legacy") {
	const [{ stdout: main }, { stdout: builder }] = await Promise.all([
		execFileAsync("npm", ["view", "@rivet-dev/dynamic-apps@latest", "version"]),
		execFileAsync("npm", [
			"view",
			"@agentos-software/apps-builder@latest",
			"version",
		]),
	]);
	if (main.trim() !== builder.trim()) {
		throw new Error(
			`legacy package versions differ: apps=${main.trim()} builder=${builder.trim()}`,
		);
	}
	version = main.trim();
}

if (!version || !/^\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?$/.test(version)) {
	throw new Error(`invalid release version: ${version ?? "<empty>"}`);
}

const sanitize = (value) =>
	value
		.toLowerCase()
		.replace(/[^a-z0-9._-]+/g, "-")
		.replace(/^[._-]+|[._-]+$/g, "")
		.slice(0, 64);

let tag = args.tag ?? "auto";
if (tag === "auto") {
	const prerelease = version.split("-")[1]?.split(".")[0];
	tag = prerelease === "rc" || prerelease === "next" ? prerelease : "latest";
} else if (tag === "preview") {
	tag = `preview-${sanitize(args.branch ?? "branch")}`;
}
if (!/^[a-z0-9][a-z0-9._-]{0,127}$/.test(tag)) {
	throw new Error(`invalid npm dist-tag: ${tag}`);
}

const output = [
	`version=${version}`,
	`npm_tag=${tag}`,
	`real_release=${!tag.startsWith("preview-")}`,
];
if (process.env.GITHUB_OUTPUT) {
	await appendFile(process.env.GITHUB_OUTPUT, `${output.join("\n")}\n`);
} else {
	console.log(output.join("\n"));
}
