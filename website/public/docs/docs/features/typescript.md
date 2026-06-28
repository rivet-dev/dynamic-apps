# TypeScript

Compiling and type-checking TypeScript inside the sandbox.

`@secure-exec/typescript` runs the TypeScript compiler **inside the sandbox**. The `typescript` package is projected into the VM's virtual filesystem and every `createProgram`/`emit` call happens in the guest, so untrusted TypeScript never compiles or runs on the host.

**Why compile in the sandbox:** type-checking attacker-controlled source is a **CPU and memory amplification** vector. A small malicious type (recursive conditional types, deeply nested generics) can make `tsc` spin for seconds or balloon its heap. Because the compiler runs in a disposable VM, the blow-up is contained by the sandbox boundary, and you can bound each run with a `timeout` instead of hanging or OOM-ing the host.

## Install

```bash
npm install @secure-exec/typescript secure-exec
```

## Compile and type-check a source string

Prints:

```
Compiled TypeScript to JavaScript inside the sandbox.
exitCode: 0
guest stdout:
hello secure-exec #1
hello secure-exec #2
hello secure-exec #3
type check success: false
  error TS2322 (line 1): Type 'string' is not assignable to type 'number'.
OK: TypeScript compiled and type-checked inside the sandbox.
```

Each diagnostic is structured (`code`, `category`, `message`, and a `line`/`column` when available), and `success` is `false` whenever any diagnostic is an error, so you can branch on results without parsing compiler text.

## Compile a tsconfig.json project

To compile a whole project, seed a `tsconfig.json` and its sources with `files` (keyed by absolute guest path), then call `compileProject`. You can also project host directories into the VM with `mounts` instead of inlining them.

```ts
import { createTypeScriptTools } from "@secure-exec/typescript";

const tools = createTypeScriptTools({
  files: {
    "/root/tsconfig.json": JSON.stringify({
      compilerOptions: { strict: true, target: "ES2022", module: "ESNext" },
      include: ["src"],
    }),
    "/root/src/index.ts": "export const answer: number = 42;\n",
  },
});

// Compile every file the tsconfig includes, emitting into the VM filesystem.
const compiled = await tools.compileProject({ cwd: "/root" });
console.log("project compiled:", compiled.success);
console.log("emitted:", compiled.emittedFiles);

// Or type-check only, without emitting any output.
const checked = await tools.typecheckProject({ cwd: "/root" });
console.log("project type-checks:", checked.success);
```

`compileProject` emits into the VM's virtual filesystem (`emittedFiles` lists the paths written). To pull that output back to the host, run the compile on a `NodeRuntime` directly and read the files with `rt.readFile`.

## Bounding untrusted compiles

`compileSource`/`typecheckSource` manage their own VM, so the compile is already contained. To put a wall-clock bound on running the *emitted* output (or your own compiler driver), run it on a `NodeRuntime` with a `timeout` or `AbortSignal`:

```ts
const emitted = await tools.compileSource({ sourceText });

const rt = await NodeRuntime.create();
try {
  const out = await rt.exec(emitted.outputText ?? "", {
    timeout: 5_000,
    signal: AbortSignal.timeout(10_000),
  });
  console.log(out.exitCode);
} finally {
  await rt.dispose();
}
```

On timeout or abort the guest process is killed inside the VM, so a runaway compile can never hang the host.