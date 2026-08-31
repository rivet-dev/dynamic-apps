# Dynamic Apps retained API contract

Status: normative rewrite contract  
Baseline: `@rivet-dev/dynamic-apps@0.2.15`, JJ `xuymorrq`, commit `baca1719`  
Scope: `appsRouter`, `deployApp`, and structured log delivery

This file is the source of truth that must be written and verified against the
old implementation before its internals are deleted. It deliberately does not
preserve any other root export, subpath export, actor name except the private
state actor identity required by `deployApp`'s injected client, inspector, or
implementation-specific response header.

The implementation gate is a bidirectional compile-time signature test, runtime
router characterization tests, deploy characterization tests, and a packed
package export/declaration snapshot. A human review of this file alone is not a
sufficient compatibility check.

The declaration below has been checked with the TypeScript compiler against the
built 0.2.15 declaration at baseline commit `baca1719`. Both `appsRouter` and
`deployApp` are assignable in both directions. The permanent characterization
test required below must preserve that result after the old implementation is
deleted.

## Exact root module

The rewritten package root exports exactly three runtime values:

```ts
export { appsRouter, deployApp, setDynamicAppsLogHandler };
```

The `./advanced` export is removed. Named exports including `setup`,
`setupApps`, `createAppsRouter`, error classes, actor definitions, routing
client types, deployment types, release types, and scaling types are removed.
Types needed to describe `deployApp` remain visible inside the generated
declaration but are not named package exports.

The expected generated declaration is structurally equivalent to:

```ts
import type { Hono } from "hono";
import type { BlankEnv, BlankSchema } from "hono/types";

interface AppScaling {
	minReplicas?: number;
	maxReplicas?: number;
	targetConcurrency?: number;
}

interface DeployAppBase {
	appId: string;
	createNamespace?: boolean;
	regions?: string[];
	scaling?: AppScaling;
}

type DeployAppInput =
	| (DeployAppBase & {
			source: URL;
			files?: never;
	  })
	| (DeployAppBase & {
			files: Record<string, string | Uint8Array>;
			source?: never;
	  });

interface Deployment {
	appId: string;
	release: string;
	endpoint: string;
	namespace: string;
	pool: string;
	token?: string;
	regions: string[];
}

interface PreparedDeployAppInput {
	appId: string;
	files: Record<string, Uint8Array>;
	regions?: string[];
	scaling?: AppScaling;
}

interface DeployAppOptions {
	client?: {
		agentOSAppsApp: {
			get?(key: string | string[]): {
				deploy(
					input: PreparedDeployAppInput,
				): Promise<
					Deployment & {
						appActorId: string;
						usesRivetKit: boolean;
					}
				>;
			};
			getOrCreate(key: string | string[]): {
				deploy(
					input: PreparedDeployAppInput,
				): Promise<
					Deployment & {
						appActorId: string;
						usesRivetKit: boolean;
					}
				>;
			};
		};
	};
}

export declare function deployApp(
	input: DeployAppInput,
	options?: DeployAppOptions,
): Promise<Deployment>;

export declare const appsRouter: Hono<BlankEnv, BlankSchema, "/">;

export type DynamicAppsLogLevel = "debug" | "info" | "warn" | "error";
export type DynamicAppsLogSource =
	| "application"
	| "actor"
	| "build"
	| "runtime";

export interface DynamicAppsLogEvent {
	version: 1;
	timestamp: number;
	level: DynamicAppsLogLevel;
	source: DynamicAppsLogSource;
	message: string;
	appId?: string;
	release?: string;
	requestId?: string;
	actorId?: string;
	stream?: "stdout" | "stderr";
	metadata?: Readonly<Record<string, string | number | boolean | null>>;
}

export type DynamicAppsLogHandler = (
	event: Readonly<DynamicAppsLogEvent>,
) => void;

export declare function setDynamicAppsLogHandler(
	handler: DynamicAppsLogHandler | undefined,
): void;
```

There are no overloads. The optional structural `client` argument is part of
the callable `deployApp` type even though `DeployAppOptions` is not a named root
export.

## `deployApp` contract

### Input preparation

- Exactly one of `source` and `files` is accepted by the TypeScript union.
- `appId` is 1–63 lowercase ASCII letters, digits, or hyphens and begins with a
  letter or digit.
- `source` is a `file:` URL to a real, non-symlink directory.
- `.git`, `.agentos`, and `node_modules` directories are ignored recursively.
- Other symlinks and non-regular files are rejected.
- Paths are normalized, relative, non-empty, contain no NUL or `..` segment,
  and are at most 1,024 UTF-8 bytes.
- At most 2,000 source files, 2 MiB per file, and 4 MiB total source are
  accepted.
- String values are UTF-8 encoded. Byte-array inputs are copied before being
  sent to the actor.
- File traversal and object keys are normalized in deterministic lexical order.

### Source-to-bundle behavior

- A non-static deployment contains `package.json`; the static-only
  no-`package.json` fallback is removed below.
- The source entrypoint is the first usable value from `package.json` exports
  (`"."`, then `import`, then `default` recursively), `main`, or these existing
  candidates in order: `src/index.mjs`, `src/index.js`, `index.mjs`,
  `index.js`.
- If `scripts.build` is a string, `npm run build` runs before the declared or
  inferred entrypoint is bundled. A build script without a declared/inferred
  handler entrypoint is rejected; the old implicit static `dist` fallback is
  removed.
- Dependencies and dev dependencies are limited to 256 total. With a
  `package-lock.json`, install uses `npm ci`; without one it uses
  `npm install`. Both use `--install-strategy=shallow --include=dev
  --omit=optional --omit=peer --legacy-peer-deps --no-audit --no-fund
  --maxsockets=16 --loglevel=error`, and are followed by `npm prune --omit=dev
  --omit=optional --omit=peer --legacy-peer-deps`.
- Install, build, and packaging have a 15-minute timeout and capture at most 2
  MiB of diagnostic output. The build filesystem is limited to 2 GiB.
- The final AOSP package is limited to 64 MiB, 4,096 files, and 32 MiB per
  file. Its direct entrypoint is a self-contained Node-targeted ESM dispatcher
  stored as `direct/main.mjs`; supported Node builtins resolve inside agentOS
  and the bundle is validated before activation.
- Native Node addons and static-only output are rejected in the first preview.
- A declared `rivetkit` dependency enables a second, platform-linked
  `actor/main.mjs` bundle. RivetKit itself is supplied by the host rather than
  installed into every app artifact. Both bundles must validate before the
  release can activate.

### Rivet and actor call

- The app state actor key is exactly `[input.appId]` and its client property
  remains `agentOSAppsApp` so the structural injected-client contract works.
- If an injected client implements `agentOSAppsApp.get`, deployment first calls
  `get([input.appId]).deploy(preparedInput)` so an existing stable actor is
  resolved without a datacenter creation constraint. Only an actor-scoped
  `not_found` falls back to
  `getOrCreate([input.appId]).deploy(preparedInput)`. A client that implements
  only the retained required `getOrCreate` method keeps the original exact call
  shape. The rewrite does not add a second deployment protocol or new Dynamic
  Apps credential.
- Every app is provisioned into a stable, isolated namespace before its build is
  activated. `createNamespace` remains accepted as a deprecated compatibility
  option but no longer changes behavior.
- Rivet Cloud deployments require the server-side `RIVET_CLOUD_TOKEN`
  management credential. App-defined actors use namespace-scoped credentials;
  the management credential is never returned to callers or app code.
- The default client is created lazily and reused process-wide.
- `no_runner_config_configured` is retried every 50 ms for no more than 15
  seconds. Other failures propagate.
- A successful call means the build artifact is completely persisted and the
  release is active. It no longer means a replica was warmed.
- A failed build or incomplete artifact write does not replace the previous
  active release.
- Identical built artifact bytes plus normalized regions/scaling,
  namespace/runtime metadata, and packaging identity produce the same opaque
  release ID within one packaging version. Unlocked dependency resolution or a
  nondeterministic build may produce a different artifact and therefore a new
  release even from identical source. The old hash algorithm is not itself
  public and changes with the runtime format.

### Compatibility fields

- `regions` remains accepted, deduplicated in input order, validated, stored,
  and returned. There must be 1–8 values matching
  `[a-z0-9][a-z0-9-]{0,62}`. The default is the state actor's current region,
  falling back to `default`. It does not place local execution in a remote
  region.
- `scaling` remains accepted and validated. Defaults are `minReplicas: 0`,
  `maxReplicas: 128`, and `targetConcurrency: 8`; bounds are 0–128, 1–128, and
  1–1,024 respectively, with `minReplicas <= maxReplicas`. It is compatibility
  metadata for direct HTTP and has no deleted scaler/replica effect.
- `endpoint`, `namespace`, deterministic `pool`, and an optional namespace-scoped
  publishable `token` are returned. The pool is
  `agentos-apps-${sha256(appId).slice(0, 16)}`. Direct HTTP does not execute in
  that pool. Actor-enabled apps use it as their stable app actor runner pool.
- `createNamespace` remains source-compatible but is deprecated and ignored.

### Result

The resolved value contains exactly these enumerable keys:

```ts
{
	appId: string;
	release: string;
	endpoint: string;
	namespace: string;
	pool: string;
	token?: string;
	regions: string[];
}
```

## `appsRouter` contract

- The value remains a Hono router with `Hono<BlankEnv, BlankSchema, "/">` type.
- It handles every HTTP method up to 256 ASCII token bytes for `/:appId` and
  `/:appId/*` and still works when mounted under a Hono prefix. The byte limit
  is an intentional first-preview transport bound; it does not restrict the
  standard method set.
- A bare app path redirects with status 308 to the same URL plus `/`, preserving
  its query string, before any actor/cache operation.
- A canonical app root is delivered to the application as `/`.
- Descendant paths remove the Hono mount and app ID while preserving the query,
  scheme, authority, accepted method, headers, cancellation/abort semantics, and
  buffered body.
- Invalid app IDs fail before actor lookup or cache work.
- The serialized absolute request URL is limited to 16 KiB of UTF-8. This is an
  intentional first-preview limit so every accepted request fits the bounded
  transport envelope.
- Request bodies are limited to 1 MiB even without `Content-Length`.
- Response bodies are limited to 4 MiB. Version one intentionally buffers the
  body and does not preserve streaming delivery timing.
- The first preview adds an explicit envelope bound: at most 256 request header
  pairs and 64 KiB total UTF-8 header names/values; responses have the same
  header limit plus a 1 KiB UTF-8 status-text limit. This is an intentional new
  limit required by envelope serialization.
- `x-agentos-app-region` selects a configured release region for compatibility
  and is stripped before application execution. It does not change the
  physical placement of the process-local executor.
- GET and HEAD requests have no forwarded body.
- Hop-by-hop headers, headers named by `Connection`, `x-rivet-token`, and
  private Dynamic Apps callback headers are not exposed to the application.
- Status, status text, ordinary headers, duplicate `Set-Cookie`, and body bytes
  are preserved within the buffering limits.
- HEAD and statuses 204, 205, and 304 return no body.

These ordinary routing outcomes are retained as direct plain-text responses:

| Condition | HTTP status | Body |
| --- | ---: | --- |
| no active ready release | 503 | `Dynamic App has no active release` |
| requested region is not configured | 421 | `Dynamic App is not deployed in requested region <region>` |
| release has no configured region | 503 | `Dynamic App has no configured region` |
| absolute request URL exceeds 16 KiB | 414 | `Request URL exceeds Dynamic Apps limit` |
| request method exceeds 256 bytes | 400 | `Request method exceeds Dynamic Apps limit` |
| request body exceeds 1 MiB | 413 | `Request body exceeds Dynamic Apps limit` |
| request headers exceed the new envelope limit | 431 | `Request headers exceed Dynamic Apps limit` |
| application handler throws before producing a response | 500 | `Internal Server Error` |

URL/method size and invalid app ID are checked before actor/cache work. Invalid
HTTP-token syntax remains rejected by the Fetch implementation before routing.
The requested-region check happens after the active-release lookup and before
the request body is read. These responses are not converted to the JSON
exception format.

Thrown or rejected errors caught by the router use the existing JSON shape:

```json
{
	"error": {
		"code": "agentos_apps_error_code",
		"message": "message"
	}
}
```

The router preserves this exception mapping:

| Error | HTTP status |
| --- | ---: |
| `agentos_apps_invalid_app_id` | 400 |
| `agentos_apps_not_deployed` | 404 |
| `agentos_apps_region_not_deployed` | 404 |
| `agentos_apps_request_limit` | 413 |
| other `agentos_apps_*` | 503 |
| unknown/untyped error | 500 |

Replica identity/count/cold-start headers and benchmark timing headers are not
part of the retained API.

## App-defined RivetKit actor contract

An application that declares `rivetkit` may export
`const registry = setup(...)` and may retain its `registry.start()` call. The
platform suppresses that call while importing the managed actor bundle. The
same app must still provide a valid direct default fetch handler.

On activation, deployment configures the returned `namespace` and `pool` with
an authenticated serverless callback to the private `agentOSAppsApp` actor.
Failure to configure that runner rolls the active pointer back. The callback
accepts only Rivet Engine traffic with the per-app secret and only the active
actor-enabled release.

The callback lazily verifies and extracts `actor/main.mjs`, then caches one
worker thread per active release. The worker uses the platform's pinned
RivetKit WebAssembly runtime and the app namespace/pool connection. Actor
state, actions, events, connections, request/response streaming, backpressure,
and cancellation use the ordinary RivetKit protocol. Worker entries are
bounded by count, V8 heap, idle TTL, callback body size, and container memory
pressure. They do not receive the deployment actor's control credential.

Runner configuration uses the ordinary `RIVET_ENDPOINT` credential by default.
`DYNAMIC_APPS_CONTROL_TOKEN` can provide a separate token with datacenter-list
and runner-config create/update permissions; it is used only for those control
calls and is never included in the actor worker environment.

The actor worker uses `RIVET_PUBLIC_ENDPOINT` when present. If
`RIVET_ENDPOINT` contains credentials and no public endpoint is configured,
actor-enabled activation fails instead of forwarding the host secret. URL auth
is split into explicit endpoint, namespace, and publishable-token fields before
the worker creates its RivetKit registry.

Actor bundle execution shares a process with the host and is not a mutually
hostile-code security boundary. Direct HTTP remains inside agentOS and never
enters the actor worker.

## Deliberate runtime narrowing

The names and call shapes above are preserved, but the first preview requires a
bundled ESM entrypoint whose default export is an object with a `fetch` method
or a fetch function:

```ts
export default {
	async fetch(request: Request): Promise<Response> {
		return new Response("ok");
	},
};
```

The following old behaviors are intentionally removed:

- static-only `index.html` packages;
- named `fetch` exports;
- WebSockets and streaming request/response bodies;
- replica scaling, admission leases, rolling replica replacement, and warm
  execution actors.

Default fetch functions and application-exported RivetKit registries remain
supported. Streaming in the list above refers only to ordinary direct HTTP;
the RivetKit actor callback remains streaming.

Dependencies that the builder can bundle into the single entrypoint remain
allowed. Native Node addons remain unsupported.

Deployment validates the bundled dispatcher in its existing disposable build
VM before activation.
Top-level import failure or a default export without a callable `fetch` fails
the deployment and leaves the previous active release unchanged. Application
handler exceptions occur at request time. Because the new path buffers before
constructing the host response, response-limit, timeout, invalid-response, and
execution-limit failures return the JSON exception shape; this is an explicit
replacement for the old mid-stream failure behavior.

An oversized response header/status envelope fails with
`agentos_apps_response_header_limit` in the JSON exception shape. The agentOS
request ABI enforces the logical body and header maxima before and after base64
expansion.

The retained deploy/source/control error codes are locked by golden tests,
including `agentos_apps_invalid_app_id`, `agentos_apps_invalid_source`,
`agentos_apps_source_symlink`, `agentos_apps_source_file_type`,
`agentos_apps_file_limit`, `agentos_apps_file_size_limit`,
`agentos_apps_source_limit`, `agentos_apps_invalid_files`,
`agentos_apps_file_count_limit`, `agentos_apps_duplicate_file_path`,
`agentos_apps_invalid_file`, `agentos_apps_entrypoint_not_found`,
`agentos_apps_invalid_package_json`, `agentos_apps_dependency_limit`,
`agentos_apps_invalid_regions`, `agentos_apps_invalid_region`,
`agentos_apps_invalid_scaling`, `agentos_apps_invalid_config`,
`agentos_apps_app_id_mismatch`, `agentos_apps_namespace_changed`,
`agentos_apps_build_write_failed`, `agentos_apps_install_failed`,
`agentos_apps_build_failed`, `agentos_apps_pack_failed`,
`agentos_apps_build_artifact_size_limit`,
`agentos_apps_build_artifact_truncated`,
`agentos_apps_native_addon_unsupported`,
`agentos_apps_control_response_limit`,
`agentos_apps_namespace_lookup_failed`,
`agentos_apps_namespace_create_failed`, and `host_registry_not_ready`.

## Mandatory verification and deletion gates

The old implementation must first pass:

1. A bidirectional TypeScript equality assertion for `typeof appsRouter` and
   `typeof deployApp` against the declarations in this file.
2. Router golden tests covering every retained behavior listed above.
3. Deploy golden tests for both union branches, the injected client, namespace
   behavior, result keys, deterministic source preparation, and failed-build
   rollback.

Record an explicit reviewed removal snapshot for all old root and subpath
exports. The old package is not expected to pass the new three-export assertion.

After the rewrite, the package must additionally pass:

1. a packed-package assertion that the runtime export key set is exactly
   `appsRouter,deployApp,setDynamicAppsLogHandler`;
2. a normalized generated `dist/index.d.ts` snapshot matching this file; and
3. explicit new tests for every intentional difference in “Deliberate runtime
   narrowing.”

The old-code characterization tests remain unchanged while the implementation
is deleted and rebuilt. Intentional differences use separately named tests
rather than weakening unrelated retained-contract assertions.
