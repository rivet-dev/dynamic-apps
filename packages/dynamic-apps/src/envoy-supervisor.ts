import { ENVOY_SHUTDOWN_PATH } from "@rivet-dev/dynamic-apps-core/internal";
import {
	ACTOR_HTTP_PORT,
	actorWorkerEnvironment,
	type LaunchedActorVm,
	launchActorVm,
} from "./actor-runtime.js";
import { emitDynamicAppsLog } from "./logging.js";
import { poolHasConnectedNode } from "./runtime.js";

const HEAP_LIMIT_MB = 256;
const START_TIMEOUT_MS = 60_000;
const CONNECT_TIMEOUT_MS = 60_000;
const CONNECT_POLL_MS = 250;
// Stays inside the Engine's default 30 second actor stop threshold, which bounds
// the owning actor's shutdown grace.
const SHUTDOWN_TIMEOUT_MS = 20_000;
const STOP_TIMEOUT_MS = 25_000;
const RESTART_BASE_DELAY_MS = 1_000;
const RESTART_MAX_DELAY_MS = 30_000;
// A guest that stayed up this long crashed for a new reason, so its restart
// starts from the base delay again.
const STABLE_UPTIME_MS = 60_000;

/** Everything needed to run one release of an app as an envoy. */
export interface EnvoyTarget {
	appId: string;
	release: string;
	/** Node version; higher versions drain lower ones in the same pool. */
	version: number;
	/** Endpoint the guest connects with, carrying its namespace and token. */
	endpoint: string;
	namespace: string;
	pool: string;
	/** Token for reading the pool's nodes during activation. */
	controlToken?: string;
	loadArtifact(): Promise<Uint8Array>;
}

/** The supervisor was stopped; it will not run another envoy. */
export class EnvoySupervisorStoppedError extends Error {
	constructor() {
		super("app envoy supervisor is stopped");
	}
}

interface RunningEnvoy {
	target: EnvoyTarget;
	launched: LaunchedActorVm;
	startedAt: number;
}

/**
 * Keeps one app's active release running as an envoy in an agentOS VM.
 *
 * The owning app actor creates one supervisor while it runs and stops it when
 * the actor stops. A guest that exits on its own is restarted with backoff, so
 * the app keeps hosting actors without any request having to arrive first.
 */
export class AppEnvoySupervisor {
	#current?: RunningEnvoy;
	#target?: EnvoyTarget;
	#stopped = false;
	#restartAttempt = 0;
	#restartTimer?: ReturnType<typeof setTimeout>;
	#transition: Promise<void> = Promise.resolve();

	/**
	 * Runs `target`, replacing any other release, then drains the previous
	 * release's envoy. With `waitForConnection`, resolves only once the new
	 * envoy has connected to the Engine and rejects, leaving the previous
	 * release serving, if it never does. A target older than the running one is
	 * ignored, so a late wake cannot roll back a newer activation.
	 */
	activate(
		target: EnvoyTarget,
		options: { waitForConnection: boolean },
	): Promise<void> {
		return this.#serialize(async () => {
			this.#throwIfStopped();
			const current = this.#target;
			// Each version belongs to one release, so an older or conflicting
			// target is a stale request.
			if (
				current &&
				(target.version < current.version ||
					(target.version === current.version &&
						target.release !== current.release))
			) {
				return;
			}
			if (
				this.#current &&
				current?.release === target.release &&
				current.version === target.version
			) {
				return;
			}
			const launched = await this.#launch(target);
			try {
				this.#throwIfStopped();
				if (options.waitForConnection) {
					await this.#waitForConnectedNode(target, launched);
				}
			} catch (error) {
				await launched.dispose();
				throw error;
			}
			const previous = this.#current;
			this.#target = target;
			this.#current = { target, launched, startedAt: Date.now() };
			this.#restartAttempt = 0;
			if (previous) await drainEnvoy(previous);
			// The guest may have exited before it became current, when its exit was
			// still ignored.
			if (launched.hasExited()) this.#onExit(launched, launched.exitError());
		});
	}

	/**
	 * Drains the running envoy and stops restarting it, within one deadline
	 * that fits the owning actor's stop grace. Work still queued past the
	 * deadline sees the stop and disposes what it launched. Safe to call
	 * repeatedly.
	 */
	async stop(): Promise<void> {
		this.#stopped = true;
		if (this.#restartTimer) clearTimeout(this.#restartTimer);
		this.#restartTimer = undefined;
		const drained = this.#serialize(async () => {
			const current = this.#current;
			this.#current = undefined;
			if (current) await drainEnvoy(current);
		});
		try {
			await withDeadline(drained, STOP_TIMEOUT_MS);
		} catch (error) {
			const current = this.#current;
			this.#current = undefined;
			emitDynamicAppsLog({
				level: "error",
				source: "actor",
				message: `Dynamic App envoy did not stop in time; disposing it: ${error instanceof Error ? error.message : String(error)}`,
				appId: this.#target?.appId,
				release: this.#target?.release,
			});
			await current?.launched.dispose();
		}
	}

	#throwIfStopped(): void {
		if (this.#stopped) throw new EnvoySupervisorStoppedError();
	}

	#serialize(run: () => Promise<void>): Promise<void> {
		const next = this.#transition.then(run);
		this.#transition = next.catch(() => {});
		return next;
	}

	async #launch(target: EnvoyTarget): Promise<LaunchedActorVm> {
		const launched = await launchActorVm({
			appId: target.appId,
			release: target.release,
			artifact: await target.loadArtifact(),
			environment: envoyEnvironment(target),
			heapLimitMb: HEAP_LIMIT_MB,
			startTimeoutMs: START_TIMEOUT_MS,
		});
		void launched.exited.then((error) => this.#onExit(launched, error));
		return launched;
	}

	/**
	 * Waits until the Engine lists `target`'s node as connected. Fails early if
	 * the guest exits or the supervisor stops, since neither can lead to a
	 * connection.
	 */
	async #waitForConnectedNode(
		target: EnvoyTarget,
		launched: LaunchedActorVm,
	): Promise<void> {
		const deadline = Date.now() + CONNECT_TIMEOUT_MS;
		let lastError: unknown;
		for (;;) {
			this.#throwIfStopped();
			if (launched.hasExited()) {
				throw new Error(
					`Dynamic App envoy for release ${target.release} exited before connecting: ${launched.exitError().message}`,
				);
			}
			try {
				const connected = await poolHasConnectedNode({
					endpoint: new URL(target.endpoint).origin,
					namespace: target.namespace,
					pool: target.pool,
					token: target.controlToken,
					version: target.version,
				});
				if (connected) return;
				lastError = undefined;
			} catch (error) {
				// A failed listing says nothing about the envoy; keep waiting.
				lastError = error;
			}
			if (Date.now() >= deadline) {
				const listing = lastError
					? ` The last node listing failed: ${lastError instanceof Error ? lastError.message : String(lastError)}.`
					: "";
				throw new Error(
					`Dynamic App envoy for release ${target.release} did not connect to the Engine within ${CONNECT_TIMEOUT_MS}ms. Check that RIVET_PUBLIC_ENDPOINT, or the namespace token on Rivet Cloud, carries a token the Engine accepts for envoys.${listing}`,
				);
			}
			await new Promise((resolve) => setTimeout(resolve, CONNECT_POLL_MS));
		}
	}

	#onExit(launched: LaunchedActorVm, error: Error): void {
		const current = this.#current;
		if (this.#stopped || current?.launched !== launched) return;
		this.#current = undefined;
		void launched.dispose();
		if (Date.now() - current.startedAt >= STABLE_UPTIME_MS) {
			this.#restartAttempt = 0;
		}
		this.#scheduleRestart(error);
	}

	#scheduleRestart(error: Error): void {
		if (this.#stopped) return;
		const delay = Math.min(
			RESTART_BASE_DELAY_MS * 2 ** this.#restartAttempt,
			RESTART_MAX_DELAY_MS,
		);
		this.#restartAttempt += 1;
		const target = this.#target;
		emitDynamicAppsLog({
			level: "error",
			source: "actor",
			message: `Dynamic App envoy stopped unexpectedly; restarting in ${delay}ms: ${error.message}`,
			appId: target?.appId,
			release: target?.release,
		});
		this.#restartTimer = setTimeout(() => {
			this.#restartTimer = undefined;
			void this.#restart();
		}, delay);
		this.#restartTimer.unref?.();
	}

	#restart(): Promise<void> {
		return this.#serialize(async () => {
			const target = this.#target;
			if (this.#stopped || this.#current || !target) return;
			let launched: LaunchedActorVm;
			try {
				// A restart needs no connection check: no other release waits on it.
				launched = await this.#launch(target);
			} catch (error) {
				this.#scheduleRestart(
					error instanceof Error ? error : new Error(String(error)),
				);
				return;
			}
			if (this.#stopped) {
				await launched.dispose();
				return;
			}
			this.#current = { target, launched, startedAt: Date.now() };
			if (launched.hasExited()) this.#onExit(launched, launched.exitError());
		});
	}
}

/** Environment for a guest that runs the release's registry as an envoy. */
function envoyEnvironment(target: EnvoyTarget): Record<string, string> {
	const environment = actorWorkerEnvironment({
		endpoint: target.endpoint,
		namespace: target.namespace,
		pool: target.pool,
	});
	const strings: Record<string, string> = {};
	for (const [name, value] of Object.entries(environment)) {
		if (typeof value === "string") strings[name] = value;
	}
	return {
		...strings,
		DYNAMIC_APPS_RUNNER_MODE: "envoy",
		RIVET_NODE_VERSION: String(target.version),
	};
}

/**
 * Asks the guest registry to stop its actors so the Engine reallocates them,
 * then disposes the VM. A guest that does not finish in time is disposed
 * anyway; the Engine then recovers its actors after the node lost threshold.
 */
async function drainEnvoy(envoy: RunningEnvoy): Promise<void> {
	const { launched, target } = envoy;
	try {
		// The guest exits once its registry has drained, which can race the
		// reply, so its exit is the drain signal.
		void launched.vm
			.fetch(
				ACTOR_HTTP_PORT,
				new Request(`http://dynamic-app.internal${ENVOY_SHUTDOWN_PATH}`, {
					method: "POST",
				}),
			)
			.catch(() => {});
		await withDeadline(launched.exited, SHUTDOWN_TIMEOUT_MS);
	} catch (error) {
		emitDynamicAppsLog({
			level: "error",
			source: "actor",
			message: `Dynamic App envoy did not drain cleanly: ${error instanceof Error ? error.message : String(error)}`,
			appId: target.appId,
			release: target.release,
		});
	} finally {
		await launched.dispose();
	}
}

async function withDeadline<T>(promise: Promise<T>, ms: number): Promise<T> {
	let timer: ReturnType<typeof setTimeout> | undefined;
	try {
		return await Promise.race([
			promise,
			new Promise<never>((_, reject) => {
				timer = setTimeout(
					() => reject(new Error(`timed out after ${ms}ms`)),
					ms,
				);
			}),
		]);
	} finally {
		if (timer) clearTimeout(timer);
	}
}
