"use strict";

// Platform-specific resolver for the prebuilt `secure-exec-sidecar` binary.
// The binary itself ships inside one of the `@secure-exec/sidecar-<platform>`
// packages, declared as optionalDependencies at publish time so npm only
// installs the package matching the current `os`/`cpu`/`libc`.
//
// Resolution priority:
//   1. `SECURE_EXEC_SIDECAR_BIN` env var.
//   2. A `secure-exec-sidecar` binary placed next to this package.
//   3. The platform-specific `@secure-exec/sidecar-<platform>` package.

const { existsSync } = require("node:fs");
const { join, dirname } = require("node:path");

const BINARY_BASENAME = "secure-exec-sidecar";

// The on-disk binary name carries the `.exe` suffix on Windows; every other
// platform ships an extension-less ELF/Mach-O binary.
const BINARY_NAME =
	process.platform === "win32" ? `${BINARY_BASENAME}.exe` : BINARY_BASENAME;

// No runtime chmod. Platform packages are published with `npm publish`, which
// preserves the binary's 0755 executable bit.

// Detect whether the current Linux process links glibc or musl. Mirrors the
// npm `libc` field used to gate the platform packages: glibc reports a glibc
// version via `process.report`, musl does not.
function detectLinuxLibc() {
	try {
		const report = process.report?.getReport?.();
		const glibc = report?.header?.glibcVersionRuntime;
		if (glibc) return "glibc";
	} catch {
		// fall through to filesystem probe
	}
	// Fallback: presence of the musl loader implies a musl userland.
	if (existsSync("/lib/ld-musl-x86_64.so.1") || existsSync("/lib/ld-musl-aarch64.so.1")) {
		return "musl";
	}
	return "glibc";
}

function getPlatformPackageName() {
	const { platform, arch } = process;
	switch (platform) {
		case "linux": {
			const libc = detectLinuxLibc();
			if (arch === "x64")
				return libc === "musl"
					? "@secure-exec/sidecar-linux-x64-musl"
					: "@secure-exec/sidecar-linux-x64-gnu";
			if (arch === "arm64")
				return libc === "musl"
					? "@secure-exec/sidecar-linux-arm64-musl"
					: "@secure-exec/sidecar-linux-arm64-gnu";
			break;
		}
		case "darwin":
			if (arch === "x64") return "@secure-exec/sidecar-darwin-x64";
			if (arch === "arm64") return "@secure-exec/sidecar-darwin-arm64";
			break;
		case "win32":
			if (arch === "x64") return "@secure-exec/sidecar-windows-x64";
			break;
		default:
			break;
	}
	return null;
}

function getSidecarPath() {
	const override = process.env.SECURE_EXEC_SIDECAR_BIN;
	if (override) {
		if (!existsSync(override)) {
			throw new Error(
				`SECURE_EXEC_SIDECAR_BIN is set to ${override} but the file does not exist`,
			);
		}
		return override;
	}

	const localBinary = join(__dirname, BINARY_NAME);
	if (existsSync(localBinary)) {
		return localBinary;
	}

	const platformPkg = getPlatformPackageName();
	if (!platformPkg) {
		throw new Error(
			`@secure-exec/sidecar: unsupported platform ${process.platform}/${process.arch}. ` +
				"The Secure Exec sidecar supports linux (x64/arm64, glibc/musl), " +
				"macOS (x64/arm64), and Windows (x64). " +
				"Set SECURE_EXEC_SIDECAR_BIN to a local secure-exec-sidecar binary to override.",
		);
	}

	let pkgJsonPath;
	try {
		pkgJsonPath = require.resolve(`${platformPkg}/package.json`);
	} catch {
		throw new Error(
			`@secure-exec/sidecar: platform package ${platformPkg} is not installed.\n` +
				"This usually means the platform is unsupported or optionalDependencies were\n" +
				`skipped during install. Try: npm install --include=optional ${platformPkg}\n` +
				"Or set SECURE_EXEC_SIDECAR_BIN to a local secure-exec-sidecar binary.",
		);
	}

	return join(dirname(pkgJsonPath), BINARY_NAME);
}

module.exports = { getSidecarPath };
