# @secure-exec/sidecar

Platform-specific resolver for the Secure Exec native sidecar binary.

The compiled `secure-exec-sidecar` binary ships inside one of the
`@secure-exec/sidecar-<platform>` packages. npm installs only the package
matching the current `os`/`cpu`/`libc` at install time.

```js
const { getSidecarPath } = require("@secure-exec/sidecar");

const binaryPath = getSidecarPath();
```

Set `SECURE_EXEC_SIDECAR_BIN` to an absolute path to override resolution for
development or custom builds.

Supported platforms: `linux-x64-gnu`, `linux-arm64-gnu`, `darwin-x64`,
`darwin-arm64`.
