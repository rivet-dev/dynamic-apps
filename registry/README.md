# secure-exec Registry

Software packages for secure-exec VMs. This includes WASM command binaries and JavaScript agent/tool packages.

Non-software packages, including filesystem drivers like S3 and Google Drive plus sandbox providers, live under `registry/file-system/` and `registry/tool/`.

## Installation

Install individual packages:

```bash
npm install @agentos-software/coreutils @agentos-software/grep
```

Or use a meta-package for a complete set:

```bash
npm install @agentos-software/common
```

## Usage

Each package exports a descriptor with command metadata and a `commandDir` path pointing to the WASM binaries:

```typescript
import coreutils from "@agentos-software/coreutils";
import grep from "@agentos-software/grep";

export const software = [coreutils, grep];
```

## Package Types

### WASM Packages

Pre-built WebAssembly binaries that register as executable commands in the VM. Each WASM package provides one or more commands (e.g., `coreutils` provides `sh`, `cat`, `ls`, etc.). Commands are compiled from Rust and C to WASM and distributed as npm packages.

### JavaScript Packages

Node.js agent and tool packages that are projected into the VM via the ModuleAccessFileSystem overlay. These include coding agents (like PI) and CLI tools that run as Node.js scripts inside the VM.

## Packages

<!-- BEGIN PACKAGE TABLE -->
### WASM Command Packages

| Package | apt Equivalent | Description | Source | Combined Size | Gzipped |
|---------|---------------|-------------|--------|---------------|---------|
| `@agentos-software/codex-cli` | codex | OpenAI Codex command package (codex, codex-exec) | rust | 274 KiB | 118 KiB |
| `@agentos-software/coreutils` | coreutils | GNU coreutils: sh, cat, ls, cp, sort, and 80+ commands | rust | 51.4 MiB | 23.5 MiB |
| `@agentos-software/curl` | curl | curl-compatible HTTP client | rust | - | - |
| `@agentos-software/diffutils` | diffutils | GNU diffutils (diff) | rust | 120 KiB | 54.0 KiB |
| `@agentos-software/duckdb` | duckdb | DuckDB command-line interface | c | - | - |
| `@agentos-software/fd` | fd-find | fd fast file finder | rust | 901 KiB | 328 KiB |
| `@agentos-software/file` | file | file type detection | rust | 117 KiB | 49.9 KiB |
| `@agentos-software/findutils` | findutils | GNU findutils (find, xargs) | rust | 950 KiB | 348 KiB |
| `@agentos-software/gawk` | gawk | GNU awk text processing | rust | 1.11 MiB | 432 KiB |
| `@agentos-software/git` | git | git version control | rust | - | - |
| `@agentos-software/grep` | grep | GNU grep pattern matching (grep, egrep, fgrep) | rust | 2.59 MiB | 956 KiB |
| `@agentos-software/gzip` | gzip | GNU gzip compression (gzip, gunzip, zcat) | rust | 391 KiB | 194 KiB |
| `@agentos-software/http-get` | http-get | Minimal HTTP GET fetch helper | c | - | - |
| `@agentos-software/jq` | jq | jq JSON processor | rust | 699 KiB | 298 KiB |
| `@agentos-software/make` | make | GNU make build tool (planned) *(planned)* | rust | - | - |
| `@agentos-software/ripgrep` | ripgrep | ripgrep fast recursive search | rust | 912 KiB | 330 KiB |
| `@agentos-software/sed` | sed | GNU sed stream editor | rust | 1.19 MiB | 455 KiB |
| `@agentos-software/sqlite3` | sqlite3 | SQLite3 command-line interface | c | - | - |
| `@agentos-software/tar` | tar | GNU tar archiver | rust | 178 KiB | 85.4 KiB |
| `@agentos-software/tree` | tree | tree directory listing | rust | 65.8 KiB | 30.0 KiB |
| `@agentos-software/unzip` | unzip | unzip archive extraction | c | 63.0 KiB | 29.0 KiB |
| `@agentos-software/wget` | wget | GNU wget HTTP client | c | - | - |
| `@agentos-software/yq` | yq | yq YAML/JSON processor | rust | 972 KiB | 411 KiB |
| `@agentos-software/zip` | zip | zip archive creation | c | 78.8 KiB | 33.6 KiB |

### Meta-Packages

| Package | Description | Includes |
|---------|-------------|----------|
| `@agentos-software/build-essential` | Build-essential WASM command set (standard + make + git + curl) | standard, make, git, curl |
| `@agentos-software/common` | Common WASM command set (coreutils + sed + grep + gawk + findutils + diffutils + tar + gzip) | coreutils, sed, grep, gawk, findutils, diffutils, tar, gzip |
| `@agentos-software/everything` | All available WASM command packages in a single bundle | coreutils, sed, grep, gawk, findutils, diffutils, tar, gzip, curl, zip, unzip, jq, ripgrep, fd, tree, file, yq, codex |
<!-- END PACKAGE TABLE -->

## Building

All WASM command source code lives in `native/`. Requires a Rust nightly toolchain (auto-installed via `rust-toolchain.toml`).

```bash
# Build everything (WASM binaries + TypeScript packages)
make build

# Or step by step:
make build-wasm    # Compile Rust + C commands to WASM
make copy-wasm     # Copy binaries into per-package wasm/ directories
make build         # Build TypeScript (includes above steps)
```

## Publishing

All packages use date-based versioning (`0.0.{YYMMDDHHmmss}`). Publishing skips unchanged packages via content hashing.

```bash
# Dry run
make publish-dry

# Publish changed packages
make publish

# Force publish all
make publish-force
```

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for how to add new packages.

## License

Apache-2.0
