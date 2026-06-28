# Executing Code

Run guest JavaScript with exec() and run(), and return values from the sandbox.

`exec()` and `run()` are the two ways to run guest code to completion. The code
runs inside the VM as a standard ES module, so top-level `import` and top-level
`await` both work.

## Run code and capture output

`exec()` runs the code and resolves with `stdout`, `stderr`, and `exitCode`.

```ts
import { NodeRuntime } from "secure-exec";

const rt = await NodeRuntime.create();
const { stdout, exitCode } = await rt.exec(`
  import os from "node:os";
  console.log("platform:", os.platform());
`);
console.log(stdout, exitCode);
await rt.dispose();
```

## Return a value from the guest

`run<T>()` does everything `exec()` does and also decodes a JSON-serializable
value the guest hands back with the injected `globalThis.__return(value)`. If the
guest never calls it, `value` is `undefined`.

```ts
const { value } = await rt.run<{ sum: number }>(`
  globalThis.__return({ sum: 2 + 40 });
`);
console.log(value?.sum); // 42
```

## Per-run options

`exec()` and `run()` take the same per-call options: `stdin` to pipe input, `env`
and `cwd` to override the environment for one run, a `timeout` and an
`AbortSignal` to bound it, and `onStdout` / `onStderr` to stream output as it is
produced.

```ts
const result = await rt.run<string>(
  `
  let input = "";
  for await (const chunk of process.stdin) input += chunk;
  globalThis.__return(input.trim().toUpperCase());
  `,
  { stdin: "piped through stdin", timeout: 10_000 },
);
```

Guest code can also invoke host-side tools you register with the `tools` option
on `NodeRuntime.create()`: each becomes a named command the guest runs by name,
round-tripping its JSON input to a handler on the host. For the full option and
result shapes, see the [TypeScript SDK reference](/docs/sdks/typescript).

*[See Full Example](https://github.com/rivet-dev/secure-exec/tree/main/examples/docs/sdk-overview)*

## Related