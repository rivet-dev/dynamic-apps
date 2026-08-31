import { buildAppRelease, readBuildConfig } from "./build.js";
import { DynamicAppsError } from "./errors.js";
import {
	DynamicAppsExecutor,
	readExecutorConfig,
	resolveExecutorConfig,
} from "./executor.js";
import { createAppsRouter } from "./router.js";
import { prepareSource } from "./source.js";
import type {
	DynamicApps,
	DynamicAppsOptions,
	PublishReleaseInput,
} from "./types.js";

export function createDynamicApps<TDeployment, TDeployOptions = undefined>(
	options: DynamicAppsOptions<TDeployment, TDeployOptions>,
): DynamicApps<TDeployment, TDeployOptions> {
	if (
		!options ||
		typeof options.publishRelease !== "function" ||
		typeof options.loadActiveRelease !== "function" ||
		typeof options.watchActiveRelease !== "function"
	) {
		throw new DynamicAppsError(
			"dynamic_apps_invalid_config",
			"createDynamicApps requires publishRelease, loadActiveRelease, and watchActiveRelease hooks",
		);
	}
	const buildConfig = readBuildConfig(options.build);
	const executorConfig = resolveExecutorConfig(
		readExecutorConfig(),
		options.executor,
	);
	const executor = new DynamicAppsExecutor(
		{
			loadActiveRelease: options.loadActiveRelease,
			watchActiveRelease: options.watchActiveRelease,
		},
		executorConfig,
		options.vm,
		options.serverRuntime,
	);
	const appsRouter = createAppsRouter(executor);
	const inFlight = new Set<Promise<unknown>>();
	let disposed = false;
	let disposePromise: Promise<void> | undefined;

	const deployApp: DynamicApps<TDeployment, TDeployOptions>["deployApp"] = (
		input,
		deployOptions,
	) => {
		if (disposed) return Promise.reject(disposedError());
		const operation = (async () => {
			const files = await prepareSource(input);
			const built = await buildAppRelease(
				{ appId: input.appId, files },
				{
					config: buildConfig,
					artifactCache: options.artifactCache,
					logger: options.logger,
				},
			);
			if (disposed) throw disposedError();
			const publishInput: PublishReleaseInput = {
				appId: input.appId,
				buildId: built.buildId,
				artifact: {
					...built.artifact,
					bytes: new Uint8Array(built.artifact.bytes),
				},
				createdAt: Date.now(),
			};
			const result = await options.publishRelease(publishInput, deployOptions);
			executor.invalidate(input.appId);
			return result;
		})();
		inFlight.add(operation);
		void operation.finally(() => inFlight.delete(operation)).catch(() => {});
		return operation;
	};

	return {
		deployApp,
		appsRouter,
		diagnostics: () => executor.diagnostics(),
		dispose() {
			if (disposePromise !== undefined) return disposePromise;
			disposed = true;
			disposePromise = (async () => {
				await Promise.allSettled([...inFlight]);
				await executor.dispose();
			})();
			return disposePromise;
		},
	};
}

function disposedError(): DynamicAppsError {
	return new DynamicAppsError(
		"dynamic_apps_executor_disposed",
		"Dynamic Apps executor is shutting down",
	);
}
