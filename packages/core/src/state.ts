import * as protocol from "./generated-protocol.js";
import { bigIntToSafeNumber } from "./numbers.js";
import { fromGeneratedProcessSnapshotStatus } from "./protocol-maps.js";

export interface LiveGuestFilesystemStat {
	mode: number;
	size: number;
	size_u64: string;
	blocks: number;
	blocks_u64: string;
	dev: number;
	dev_u64: string;
	rdev: number;
	rdev_u64: string;
	is_directory: boolean;
	is_symbolic_link: boolean;
	atime_ms: number;
	mtime_ms: number;
	ctime_ms: number;
	birthtime_ms: number;
	ino: number;
	ino_u64: string;
	nlink: number;
	uid: number;
	gid: number;
}

export interface LiveSocketStateEntry {
	process_id: string;
	host?: string;
	port?: number;
	path?: string;
}

export interface LiveProcessSnapshotEntry {
	process_id: string;
	pid: number;
	ppid: number;
	pgid: number;
	sid: number;
	driver: string;
	command: string;
	args?: string[];
	cwd: string;
	status: "running" | "exited" | "stopped";
	exit_code?: number;
}

export function fromGeneratedGuestFilesystemStat(
	stat: protocol.GuestFilesystemStat,
): LiveGuestFilesystemStat {
	return {
		mode: stat.mode,
		size: Number(stat.size),
		size_u64: stat.size.toString(),
		blocks: Number(stat.blocks),
		blocks_u64: stat.blocks.toString(),
		dev: Number(stat.dev),
		dev_u64: stat.dev.toString(),
		rdev: Number(stat.rdev),
		rdev_u64: stat.rdev.toString(),
		is_directory: stat.isDirectory,
		is_symbolic_link: stat.isSymbolicLink,
		atime_ms: bigIntToSafeNumber(stat.atimeMs, "guest filesystem stat atime"),
		mtime_ms: bigIntToSafeNumber(stat.mtimeMs, "guest filesystem stat mtime"),
		ctime_ms: bigIntToSafeNumber(stat.ctimeMs, "guest filesystem stat ctime"),
		birthtime_ms: bigIntToSafeNumber(
			stat.birthtimeMs,
			"guest filesystem stat birthtime",
		),
		ino: Number(stat.ino),
		ino_u64: stat.ino.toString(),
		nlink: bigIntToSafeNumber(stat.nlink, "guest filesystem stat nlink"),
		uid: stat.uid,
		gid: stat.gid,
	};
}

export function fromGeneratedSocketStateEntry(
	entry: protocol.SocketStateEntry,
): LiveSocketStateEntry {
	return {
		process_id: entry.processId,
		...(entry.host !== null ? { host: entry.host } : {}),
		...(entry.port !== null ? { port: entry.port } : {}),
		...(entry.path !== null ? { path: entry.path } : {}),
	};
}

export function fromGeneratedProcessSnapshotEntry(
	entry: protocol.ProcessSnapshotEntry,
): LiveProcessSnapshotEntry {
	return {
		process_id: entry.processId,
		pid: entry.pid,
		ppid: entry.ppid,
		pgid: entry.pgid,
		sid: entry.sid,
		driver: entry.driver,
		command: entry.command,
		args: [...entry.args],
		cwd: entry.cwd,
		status: fromGeneratedProcessSnapshotStatus(entry.status),
		...(entry.exitCode !== null ? { exit_code: entry.exitCode } : {}),
	};
}
