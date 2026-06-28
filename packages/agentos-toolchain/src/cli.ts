#!/usr/bin/env node
import { existsSync, statSync } from "node:fs";
import { basename, resolve } from "node:path";
import { pack } from "./pack.js";

const USAGE = `agentos-toolchain — build agentOS packages

Usage:
  agentos-toolchain pack <npm-pkg | ./local-dir> [options]

Options:
  --agent <command>   mark a bin command as the package's ACP entrypoint
  --out <dir>         output dir for the package itself (FLAT;
                      default: ./<input-name>-package)
  --prune-native      delete unreachable native .node addons from the flat closure
  -h, --help          show this help
`;

/** Default flat output dir: ./<input-name>-package in cwd. */
function defaultOutName(source: string): string {
	if (existsSync(source) && statSync(source).isDirectory()) {
		return `./${basename(resolve(source))}-package`;
	}
	// npm spec: strip a trailing @version, then the @scope/ prefix.
	const at = source.lastIndexOf("@");
	const name = at > 0 ? source.slice(0, at) : source;
	return `./${name.replace(/^@[^/]+\//, "")}-package`;
}

function parseArgs(argv: string[]): {
	cmd: string;
	source?: string;
	agent?: string;
	out?: string;
	pruneNative: boolean;
} {
	const [cmd, ...rest] = argv;
	let source: string | undefined;
	let agent: string | undefined;
	let out: string | undefined;
	let pruneNative = false;
	for (let i = 0; i < rest.length; i++) {
		const a = rest[i];
		if (a === "--agent") agent = rest[++i];
		else if (a === "--out") out = rest[++i];
		else if (a === "--prune-native") pruneNative = true;
		else if (a === "-h" || a === "--help") {
			process.stdout.write(USAGE);
			process.exit(0);
		} else if (!a.startsWith("-") && source === undefined) source = a;
		else throw new Error(`unexpected argument: ${a}`);
	}
	return { cmd, source, agent, out, pruneNative };
}

function main(): void {
	const args = parseArgs(process.argv.slice(2));
	if (args.cmd === undefined || args.cmd === "-h" || args.cmd === "--help") {
		process.stdout.write(USAGE);
		process.exit(args.cmd === undefined ? 1 : 0);
	}
	if (args.cmd !== "pack") {
		throw new Error(`unknown command "${args.cmd}" (only "pack" is supported)`);
	}
	if (!args.source) throw new Error("pack requires a <npm-pkg | ./local-dir> argument");

	const result = pack({
		source: args.source,
		out: resolve(args.out ?? defaultOutName(args.source)),
		agent: args.agent,
		pruneNative: args.pruneNative,
	});
	process.stdout.write(
		`packed ${result.name}@${result.version} → ${result.packageDir}\n` +
			`  commands: ${result.commands.join(", ")}\n`,
	);
}

try {
	main();
} catch (error) {
	process.stderr.write(`error: ${error instanceof Error ? error.message : String(error)}\n`);
	process.exit(1);
}
