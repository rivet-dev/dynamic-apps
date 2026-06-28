# Child Processes

Spawn child processes from sandboxed code.

Two ways to run processes in Secure Exec:

- **Guest `node:child_process`**: guest code spawns commands inside the VM. Every child is a kernel-managed process, never a real host process.
- **`rt.spawn(code)`**: the host starts a long-running guest program and gets a live handle (`pid`, `writeStdin`, `kill`, `wait`, `exitCode`).

## Guest child_process

Guest code uses the standard `node:child_process` module to spawn commands available in the VM (`sh`, `node`, and the mounted coreutils):

Output:

```
exitCode: 0
guest stdout:
sh output: hello from a child process
child node version: v22.0.0
```

- `execFileSync` is used for brevity; the async/streaming APIs (`spawn`, `exec`, `execFile`) also work for incremental stdout/stderr or writing to a child's stdin.
- Children run any command provided by the mounted runtimes. By default that is WASM-backed `sh` + coreutils and V8-backed `node`.
- Point at a different set of WASM command binaries with `commandsDir`:

```ts
const rt = await NodeRuntime.create({
  commandsDir: "/path/to/wasm/commands",
});
```

<Note>Child processes always run inside the kernel. Guest code cannot reach a real host process or host binary; `node:child_process` only sees the commands the VM mounts.</Note>

### Where the commands come from

The guest `sh` and the coreutils it drives ship as WASM binaries. The kernel cannot spawn any guest process without them, so they are mounted through the WASM runtime at boot. This is how `node:child_process` and the shell work inside the VM with no host processes ever involved.

The `commandsDir` create option overrides where those WASM command binaries are loaded from. When unset, the runtime resolves a directory using the first match in this order:

1. an explicit `commandsDir` option,
2. the `SECURE_EXEC_WASM_COMMANDS_DIR` environment variable,
3. the in-repo build output (`registry/native/target/wasm32-wasip1/release/commands`), present only in developer checkouts,
4. the commands vendored into the installed `@secure-exec/core` package (published installs).

The in-repo build output wins over the bundled copy so local edits are picked up without re-vendoring; a fresh `npm install` has no in-repo path and falls through to the vendored commands. See the [TypeScript SDK reference](/docs/sdks/typescript) for the full create-option shape.

## Long-running guests with rt.spawn

`rt.exec` runs to completion and returns captured output. `rt.spawn(code)` returns a live handle immediately while the guest keeps running. It is the building block for dev servers and other long-lived guests.

```ts
const proc = await rt.spawn(`
  process.stdin.on("data", (chunk) => {
    process.stdout.write("got: " + chunk.toString());
  });
`);

proc.writeStdin("hello\n");   // feed stdin
proc.closeStdin();            // signal end-of-input

const exitCode = await proc.wait();
console.log(proc.pid, exitCode);
```

See the [TypeScript SDK reference](/docs/sdks/typescript) for the full `NodeRuntimeProcess` and `NodeRuntimeSpawnOptions` shapes. Stream output by passing `onStdout` / `onStderr`, which receive raw `Uint8Array` chunks:

```ts
const proc = await rt.spawn("setInterval(() => console.log('tick'), 100)", {
  onStdout: (chunk) => process.stdout.write(new TextDecoder().decode(chunk)),
});
// ... later
proc.kill();              // SIGTERM
await proc.wait();
```

### Driving a guest server

Spawn a server, wait for it to listen, then drive requests into it with `rt.fetch`, entirely inside the VM, even when guest network egress is denied:

```ts
const server = await rt.spawn(`
  import http from "node:http";
  http.createServer((_, res) => res.end("ok")).listen(3000);
`);

const listener = await rt.waitForListener({ port: 3000 });
const res = await rt.fetch(listener.port ?? 3000, { path: "/" });
console.log(res.status, res.body); // 200 ok

server.kill();
await server.wait();
```

## Underlying process model

- The kernel process table, signals, and shell that back `node:child_process` and `rt.spawn` are documented in agentOS: [Processes & Shell](https://agentos-sdk.dev/docs/processes).