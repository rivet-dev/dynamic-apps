# Rust SDK

Install the Secure Exec Rust client and find its generated docs.rs reference.

The `secure-exec-client` crate is the Rust SDK. It speaks the same sidecar wire protocol as the TypeScript client, so every capability reachable from `secure-exec` is also reachable from Rust through `SidecarProcess`.

## Install

```bash
cargo add secure-exec-client
```

Or add it to `Cargo.toml`:

```toml
[dependencies]
secure-exec-client = "*"
```

## API reference

The full reference is generated with rustdoc and published to docs.rs. Use it as the source of truth for signatures and types.