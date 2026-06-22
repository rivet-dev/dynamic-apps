#!/usr/bin/env node

import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const DEFAULT_INPUT = fileURLToPath(
	new URL("../../core/fixtures/alpine-defaults.json", import.meta.url),
);
const DEFAULT_OUTPUT = fileURLToPath(
	new URL("../../core/fixtures/base-filesystem.json", import.meta.url),
);

const BASE_HOSTNAME = "secure-exec";
const BASE_USER = "agentos";
const BASE_HOME = `/home/${BASE_USER}`;
const BASE_UID = 1000;
const BASE_GID = 1000;

// Non-Alpine directories the secure-exec base layer always provides on top of the
// captured snapshot. `/workspace` is the default agent working directory (cwd) and
// the conventional mount root; it is kept separate from `$HOME` (/home/agentos),
// which is never a mount.
const EXTRA_DIRECTORIES = [
	{ path: "/workspace", type: "directory", mode: "755", uid: BASE_UID, gid: BASE_GID },
];

function readJson(pathname) {
	return JSON.parse(readFileSync(pathname, "utf-8"));
}

function normalizeEntry(entry) {
	if (entry.path === "/etc/hostname" && entry.type === "file") {
		return {
			...entry,
			content: `${BASE_HOSTNAME}\n`,
		};
	}

	return entry;
}

function withExtraDirectories(entries) {
	const existing = new Set(entries.map((entry) => entry.path));
	const additions = EXTRA_DIRECTORIES.filter((entry) => !existing.has(entry.path));
	return [...entries, ...additions];
}

function buildBaseFilesystem(snapshot, inputPath) {
	return {
		source: {
			snapshotPath: path.basename(inputPath),
			image: snapshot.image,
			snapshotCreatedAt: snapshot.createdAt,
			builtAt: new Date().toISOString(),
			transforms: [
				"Normalize HOSTNAME to secure-exec",
				"Preserve the captured user-level environment and filesystem layout as the secure-exec base layer",
				"Add the non-Alpine /workspace directory (default agent working directory) owned by the base user",
			],
		},
		environment: {
			env: {
				...snapshot.environment.env,
				HOME: BASE_HOME,
				HOSTNAME: BASE_HOSTNAME,
				LOGNAME: BASE_USER,
				USER: BASE_USER,
			},
			prompt: snapshot.environment.prompt,
		},
		filesystem: {
			entries: withExtraDirectories(snapshot.filesystem.entries.map(normalizeEntry)),
		},
	};
}

function main() {
	const [inputPath = DEFAULT_INPUT, outputPath = DEFAULT_OUTPUT] = process.argv.slice(2);
	const snapshot = readJson(inputPath);
	const baseFilesystem = buildBaseFilesystem(snapshot, inputPath);

	mkdirSync(path.dirname(outputPath), { recursive: true });
	writeFileSync(outputPath, `${JSON.stringify(baseFilesystem, null, 2)}\n`);
	process.stdout.write(`Wrote ${outputPath}\n`);
}

main();
