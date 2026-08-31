import { readFileSync } from "node:fs";

const MEMORY_ADMISSION_RESERVE_BYTES = 32 * 1024 * 1024;
const MEMORY_ADMISSION_PAYLOAD_BYTES = 16 * 1024 * 1024;

export interface CgroupMemory {
	currentBytes: number;
	maxBytes: number;
}

export function capConcurrencyForMemory(input: {
	requested: number;
	contextAndVmLimitMb: number;
	memoryHighWaterPercent: number;
	currentBytes: number;
	maxBytes: number;
}): number {
	const targetBytes =
		(input.maxBytes * input.memoryHighWaterPercent) / 100 -
		MEMORY_ADMISSION_RESERVE_BYTES;
	const availableBytes = Math.max(0, targetBytes - input.currentBytes);
	const perRequestBytes =
		input.contextAndVmLimitMb * 1024 * 1024 + MEMORY_ADMISSION_PAYLOAD_BYTES;
	return Math.max(
		1,
		Math.min(input.requested, Math.floor(availableBytes / perRequestBytes)),
	);
}

export function readCgroupMemory(): CgroupMemory | undefined {
	try {
		const cgroupPath = readFileSync("/proc/self/cgroup", "utf8")
			.split("\n")
			.find((line) => line.startsWith("0::"))
			?.slice(3);
		if (!cgroupPath?.startsWith("/") || cgroupPath.includes("..")) {
			return undefined;
		}
		const directory = `/sys/fs/cgroup${cgroupPath === "/" ? "" : cgroupPath}`;
		const currentBytes = Number(
			readFileSync(`${directory}/memory.current`, "utf8").trim(),
		);
		const maxText = readFileSync(`${directory}/memory.max`, "utf8").trim();
		if (maxText === "max") return undefined;
		const maxBytes = Number(maxText);
		if (
			!Number.isFinite(currentBytes) ||
			currentBytes < 0 ||
			!Number.isFinite(maxBytes) ||
			maxBytes <= 0
		) {
			return undefined;
		}
		return { currentBytes, maxBytes };
	} catch {
		return undefined;
	}
}
