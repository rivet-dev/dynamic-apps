import { DynamicAppsError } from "./errors.js";

export function extractAospkgTextFile(
	bytes: Uint8Array,
	target: string,
): string {
	const buffer = Buffer.from(bytes.buffer, bytes.byteOffset, bytes.byteLength);
	if (
		buffer.byteLength < 16 ||
		buffer[0] !== 137 ||
		buffer.subarray(1, 4).toString("ascii") !== "AOS"
	) {
		throw new DynamicAppsError(
			"dynamic_apps_artifact_format_invalid",
			"application artifact is not an AOSP package",
		);
	}
	let offset = 16 + buffer.readUInt32LE(8) + buffer.readUInt32LE(12);
	while (offset + 512 <= buffer.byteLength) {
		const header = buffer.subarray(offset, offset + 512);
		if (header.every((value) => value === 0)) break;
		const name = tarString(header.subarray(0, 100));
		const prefix = tarString(header.subarray(345, 500));
		const path = `${prefix ? `${prefix}/` : ""}${name}`.replace(/^\.\//, "");
		const sizeText = tarString(header.subarray(124, 136)).trim();
		const size = Number.parseInt(sizeText || "0", 8);
		if (!Number.isSafeInteger(size) || size < 0) break;
		const dataOffset = offset + 512;
		const next = dataOffset + Math.ceil(size / 512) * 512;
		if (next > buffer.byteLength) break;
		if (path === target || path === `/${target}`) {
			return new TextDecoder("utf-8", { fatal: true }).decode(
				buffer.subarray(dataOffset, dataOffset + size),
			);
		}
		offset = next;
	}
	throw new DynamicAppsError(
		"dynamic_apps_artifact_entry_missing",
		`application artifact is missing ${target}`,
	);
}

function tarString(bytes: Uint8Array): string {
	const end = bytes.indexOf(0);
	return Buffer.from(end < 0 ? bytes : bytes.subarray(0, end)).toString("utf8");
}
