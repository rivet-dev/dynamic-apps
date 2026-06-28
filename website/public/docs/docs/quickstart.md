# Quickstart

Get Secure Exec running in a few minutes.

1. **Install**

   ```bash title="npm"
   npm install secure-exec
   ```

   ```bash title="bun"
   bun add secure-exec
   ```

   ```bash title="pnpm"
   pnpm add secure-exec
   ```

   ```bash title="yarn"
   yarn add secure-exec
   ```

2. **Create a runtime**

   `NodeRuntime.create()` boots a fully virtualized VM behind the native sidecar. Guest code runs inside the kernel isolation boundary with no host escapes. All options are optional: `cwd` defaults to `/workspace`, and permissions default to a secure policy that denies network access (see step 4).

   ```ts
   import { NodeRuntime } from "secure-exec";

   const runtime = await NodeRuntime.create();
   ```

3. **Run code**

   Use `run()` when you want a JSON value back; the guest calls `globalThis.__return(value)` to set it. Use `exec()` when you care about side effects and want to capture `stdout`/`stderr`/`exitCode`. Guest code runs as an ES module, so `import` and top-level `await` both work.

   ```ts title="Capture output"
   import { NodeRuntime } from "secure-exec";

   const runtime = await NodeRuntime.create();

   try {
     // exec() runs guest code for its side effects and captures the streams.
     const result = await runtime.exec(`
       console.log("hello from secure-exec");
       console.error("this goes to stderr");
     `);

     console.log("stdout:", JSON.stringify(result.stdout.trim()));
     console.log("stderr:", JSON.stringify(result.stderr.trim()));
     console.log("exitCode:", result.exitCode);
   } finally {
     await runtime.dispose();
   }
   ```

   *[See Full Example](https://github.com/rivet-dev/secure-exec/tree/main/examples/docs/quickstart)*

4. **Configure permissions (optional)**

   Guest code is **deny-by-default** for network access. Pass a `permissions` policy to `NodeRuntime.create()` to opt in; it merges over the secure default, so you only specify what you want to change:

   ```ts
   const runtime = await NodeRuntime.create({
     permissions: { network: "allow" },
   });
   ```

   See [Permissions](/docs/features/permissions) for the full scope list and merge semantics.

## Next steps