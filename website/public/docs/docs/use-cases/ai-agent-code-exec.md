# AI Agent Code Exec

Give AI agents a secure code-execution tool that runs untrusted code in a sandbox and returns structured results.

Give your agent a code-execution tool that runs untrusted, model-generated code in a fully virtualized VM. The agent writes code, it runs inside the kernel isolation boundary with no access to the host, and you get back its stdout and a structured return value.

## Run untrusted JavaScript and capture a result

`NodeRuntime.create()` boots a sandboxed VM. `rt.run()` executes the guest code and decodes whatever it passes to `globalThis.__return()`, while still capturing `stdout`, `stderr`, and `exitCode`. Use `rt.exec()` when you only need the captured output streams.

The guest code runs as a standard ES module (top-level `await` and `import` work), but it can only see the virtual filesystem and kernel-mediated syscalls, and it cannot reach the host machine.

*[See Full Example](https://github.com/rivet-dev/secure-exec/tree/main/examples/docs/uc-ai-agent-code-exec)*

Running this prints the captured stdout and the decoded return value, and shows that the guest only ever sees the sandbox:

```
exitCode: 0
stdout: computed 20 fibonacci numbers
returned value: {"fibonacci":[0,1,1,2,3,5,8,13,21,34,55,89,144,233,377,610,987,1597,2584,4181],"sum":10945}

escape attempt exitCode: 0
escape attempt stdout: guest hostname: secure-exec
```

## Wiring it into an agent tool

A single `NodeRuntime` instance can run many programs. Each `run()` / `exec()` call executes a fresh guest process. To expose this as a tool to your agent framework, hold one runtime for the session and call `rt.run(code)` from the tool's handler, returning `{ stdout, value, exitCode }` to the model. Pass a `timeout` (in milliseconds) per call to bound runaway code, and call `rt.dispose()` when the session ends.

## Sandboxing untrusted code

`NodeRuntime.create()` denies network by default and allows the virtualized `fs`, `childProcess`, `process`, and `env` scopes; any `permissions` you pass merges over that default, so an omitted scope keeps its default rather than being denied. Pass a partial policy (for example `{ network: "allow" }` to opt into the network, or a rule set on `fs` to tighten filesystem access) to adjust individual scopes.

```ts
import { NodeRuntime } from "secure-exec";

// network is already denied by default; this opts it back in while the other
// scopes keep their defaults.
const rt = await NodeRuntime.create({
  permissions: { network: "allow" },
});
```

You can also constrain the run with `env` and `cwd` on `NodeRuntime.create()`, and `env`, `cwd`, `stdin`, and `timeout` on each `run()` / `exec()` call.