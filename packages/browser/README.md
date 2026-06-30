# Secure Exec Browser

Browser driver primitives for secure-exec.

- Package: `@secure-exec/browser`
- Exports: `createBrowserDriver`, `createBrowserRuntimeDriverFactory`, `createOpfsFileSystem`, `BrowserWorkerAdapter`

## Permissions

The browser runtime gates guest I/O through a per-scope `PermissionCheck` callback
model (`fs`, `network`, `childProcess`, `env`) rather than the node runtime's
string-policy permissions. Each callback receives the request and returns a
decision (`boolean`, or `{ allowed | allow, reason? }`); the default policy should
be your own secure callbacks, not a fully-open one.

For dev/test runtimes that need everything open, the package exports convenience
presets:

- `allowAll` — aggregate `Permissions` allowing every scope.
- `allowAllFs`, `allowAllNetwork`, `allowAllChildProcess`, `allowAllEnv` — per-scope presets.
