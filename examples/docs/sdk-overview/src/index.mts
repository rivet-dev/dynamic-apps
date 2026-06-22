/**
 * SDK Overview example.
 *
 * A guided tour of the secure-exec façade:
 *   - NodeRuntime.create(options) boots a fully virtualized VM.
 *   - exec(code)  runs guest JavaScript and captures stdout/stderr/exitCode.
 *   - run(code)   runs guest JavaScript and returns a JSON value via __return().
 *   - dispose()   tears down the VM.
 *
 * Run with:
 *   SECURE_EXEC_SIDECAR_BIN=../../../target/debug/secure-exec-sidecar \
 *     npx tsx src/index.mts
 */

import { NodeRuntime } from "secure-exec";

// Boot a VM. Every option here is optional; these are the defaults made
// explicit. `cwd` defaults to "/workspace" and permissions default to allow-all.
const rt = await NodeRuntime.create({
  env: { GREETING: "hello from the VM env" },
  cwd: "/workspace",
});

try {
  // exec() runs guest code for its output. Guest code is an ES module, so
  // `import` and top-level `await` both work.
  const execResult = await rt.exec(
    `
    import os from "node:os";
    console.log("stdout:", 1 + 1);
    console.error("stderr:", process.env.GREETING);
    console.log("platform:", os.platform());
    `,
  );
  console.log("exec.stdout:  ", JSON.stringify(execResult.stdout));
  console.log("exec.stderr:  ", JSON.stringify(execResult.stderr));
  console.log("exec.exitCode:", execResult.exitCode);

  // run() returns a JSON-serializable value: the guest calls
  // globalThis.__return(value) and that value is decoded on the host.
  const runResult = await rt.run<{ sum: number; cwd: string }>(
    `
    globalThis.__return({ sum: 2 + 40, cwd: process.cwd() });
    console.log("computed in the VM");
    `,
  );
  console.log("run.value:    ", JSON.stringify(runResult.value));
  console.log("run.stdout:   ", JSON.stringify(runResult.stdout));
  console.log("run.exitCode: ", runResult.exitCode);

  // Per-run options: extra env, stdin, cwd, and a timeout (ms).
  const stdinResult = await rt.run<string>(
    `
    let input = "";
    for await (const chunk of process.stdin) input += chunk;
    globalThis.__return(input.trim().toUpperCase());
    `,
    { stdin: "piped through stdin", timeout: 10_000 },
  );
  console.log("run.value (stdin):", JSON.stringify(stdinResult.value));
} finally {
  // Always release the VM and the underlying sidecar.
  await rt.dispose();
  console.log("disposed");
}
