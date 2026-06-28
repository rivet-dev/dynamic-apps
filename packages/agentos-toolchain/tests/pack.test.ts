import { execFileSync } from "node:child_process";
import {
	chmodSync,
	lstatSync,
	mkdirSync,
	mkdtempSync,
	readFileSync,
	readlinkSync,
	rmSync,
	writeFileSync,
} from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { afterEach, describe, expect, test } from "vitest";
import { detectExecutableKind, isNativeKind, parseShebangInterpreter } from "../src/header.js";
import { findNativeAddons, pack, verifyPackageDir } from "../src/pack.js";

function hasWorkingNpm(): boolean {
	try {
		execFileSync("npm", ["--version"], { stdio: "ignore" });
		return true;
	} catch {
		return false;
	}
}
const npmOk = hasWorkingNpm();

const dirs: string[] = [];
const mkTmp = (p: string) => {
	const d = mkdtempSync(join(tmpdir(), p));
	dirs.push(d);
	return d;
};
afterEach(() => {
	for (const d of dirs.splice(0)) rmSync(d, { recursive: true, force: true });
});

describe("header detection", () => {
	test("recognizes shebang, wasm, and native magics", () => {
		expect(detectExecutableKind(Buffer.from("#!/usr/bin/env node\n"))).toBe("shebang");
		expect(detectExecutableKind(Buffer.from([0x00, 0x61, 0x73, 0x6d]))).toBe("wasm");
		expect(detectExecutableKind(Buffer.from([0x7f, 0x45, 0x4c, 0x46]))).toBe("native-elf");
		expect(detectExecutableKind(Buffer.from([0xcf, 0xfa, 0xed, 0xfe]))).toBe("native-macho");
		expect(detectExecutableKind(Buffer.from([0x4d, 0x5a, 0x90, 0x00]))).toBe("native-pe");
		expect(detectExecutableKind(Buffer.from("just text"))).toBe("unknown");
	});

	test("cafebabe Java class is not mis-detected as Mach-O", () => {
		const java = Buffer.from([0xca, 0xfe, 0xba, 0xbe, 0x00, 0x00, 0x00, 0x34]); // Java 8
		expect(detectExecutableKind(java)).toBe("unknown");
		const fat = Buffer.from([0xca, 0xfe, 0xba, 0xbe, 0x00, 0x00, 0x00, 0x02]); // Mach-O fat
		expect(detectExecutableKind(fat)).toBe("native-macho");
	});

	test("isNativeKind + shebang interpreter parsing", () => {
		expect(isNativeKind("native-elf")).toBe(true);
		expect(isNativeKind("shebang")).toBe(false);
		expect(parseShebangInterpreter(Buffer.from("#!/usr/bin/env python3\n"))).toBe("/usr/bin/env");
		expect(parseShebangInterpreter(Buffer.from("not a shebang"))).toBeNull();
	});
});

/** Build a FLAT package dir by hand (no npm) to exercise verifyPackageDir. */
function handBuiltPackage(name = "pkg", version = "1.0.0"): string {
	const pkgDir = mkTmp("agentos-pkg-");
	mkdirSync(join(pkgDir, "bin"), { recursive: true });
	mkdirSync(join(pkgDir, "node_modules"), { recursive: true });
	writeFileSync(
		join(pkgDir, "package.json"),
		`${JSON.stringify({ name, version, bin: { tool: "bin/tool" } }, null, 2)}\n`,
	);
	writeFileSync(join(pkgDir, "bin", "tool"), "#!/usr/bin/env node\nconsole.log('ok');\n");
	chmodSync(join(pkgDir, "bin", "tool"), 0o755);
	return pkgDir;
}

describe("verifyPackageDir", () => {
	test("accepts a well-formed package", () => {
		expect(() => verifyPackageDir(handBuiltPackage())).not.toThrow();
	});

	test("rejects a missing package.json", () => {
		const pkgDir = handBuiltPackage();
		rmSync(join(pkgDir, "package.json"));
		expect(() => verifyPackageDir(pkgDir)).toThrow(/package\.json/);
	});

	test("rejects native .node addons", () => {
		const pkgDir = handBuiltPackage("withaddon");
		writeFileSync(join(pkgDir, "node_modules", "evil.node"), "binary");
		expect(() => verifyPackageDir(pkgDir)).toThrow(/native \.node addon/);
		// The error must name the --prune-native escape hatch.
		expect(() => verifyPackageDir(pkgDir)).toThrow(/--prune-native/);
	});

	test("findNativeAddons locates .node files; pruning them lets verify pass", () => {
		const pkgDir = handBuiltPackage("prunable");
		const nm = join(pkgDir, "node_modules");
		mkdirSync(join(nm, "koffi", "build"), { recursive: true });
		writeFileSync(join(nm, "koffi", "build", "koffi.node"), "native");
		writeFileSync(join(nm, "clipboard.node"), "native");
		const addons = findNativeAddons(nm);
		expect(addons.length).toBe(2);
		expect(() => verifyPackageDir(pkgDir)).toThrow(/native \.node addon/);
		// Prune them (what --prune-native does) → verify now passes.
		for (const addon of addons) rmSync(addon);
		expect(() => verifyPackageDir(pkgDir)).not.toThrow();
	});

	test("rejects a headerless bin command", () => {
		const pkgDir = handBuiltPackage("bad");
		writeFileSync(join(pkgDir, "bin", "nohdr"), "plain text, no shebang");
		expect(() => verifyPackageDir(pkgDir)).toThrow(/no recognized header/);
	});

	test("rejects a native bin command", () => {
		const pkgDir = handBuiltPackage("native");
		writeFileSync(join(pkgDir, "bin", "elf"), Buffer.from([0x7f, 0x45, 0x4c, 0x46, 0, 0]));
		expect(() => verifyPackageDir(pkgDir)).toThrow(/native native-elf/);
	});
});

function makeFixture(name = "hello", version = "1.2.3"): string {
	const src = mkTmp("agentos-fixture-");
	writeFileSync(
		join(src, "package.json"),
		JSON.stringify({ name, version, bin: { hello: "bin/hello.js" } }),
	);
	mkdirSync(join(src, "bin"), { recursive: true });
	writeFileSync(join(src, "bin", "hello.js"), "#!/usr/bin/env node\nconsole.log('hi');\n");
	chmodSync(join(src, "bin", "hello.js"), 0o755);
	return src;
}

// These exercise the real `npm install` flow; gated on a working npm (present in
// any npx/CI environment, absent in this minimal pnpm-only sandbox).
describe.skipIf(!npmOk)("pack (offline, local fixture, needs npm)", () => {
	test("packs a zero-dep local package into a flat valid agentOS package", () => {
		const src = makeFixture();
		const out = mkTmp("agentos-out-");
		const result = pack({ source: src, out });

		expect(result.name).toBe("hello");
		expect(result.version).toBe("1.2.3");
		expect(result.commands).toEqual(["hello"]);
		expect(result.packageDir).toBe(out);

		// Flat output: the package lands directly in `out` (no <name>/<version>/).
		expect(JSON.parse(readFileSync(join(out, "package.json"), "utf8"))).toEqual({
			name: "hello",
			version: "1.2.3",
			bin: { hello: "bin/hello" },
		});
		const binLink = join(out, "bin", "hello");
		expect(lstatSync(binLink).isSymbolicLink()).toBe(true);
		expect(readlinkSync(binLink)).toContain("node_modules/hello/bin/hello.js");
		expect(() => verifyPackageDir(out)).not.toThrow();
	});

	test("--agent validates the entrypoint against the package commands", () => {
		const src = makeFixture("agentpkg", "0.1.0");
		const out = mkTmp("agentos-out-");
		pack({ source: src, out, agent: "hello" });
		// No agentos-package.json; the entrypoint is a real bin command + in package.json bin.
		const pkg = JSON.parse(readFileSync(join(out, "package.json"), "utf8"));
		expect(pkg.bin).toHaveProperty("hello");
		expect(lstatSync(join(out, "bin", "hello")).isSymbolicLink()).toBe(true);
		// An entrypoint that is not a command is rejected.
		expect(() => pack({ source: src, out: mkTmp("agentos-out-"), agent: "nope" })).toThrow(
			/--agent "nope" is not one of/,
		);
	});
});
