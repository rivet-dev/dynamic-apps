# syntax=docker/dockerfile:1.10.0
#
# Cross-compile the secure-exec sidecar for macOS via osxcross, on a LINUX
# runner — so the publish never depends on scarce/queued GitHub macOS runners
# (the failure mode that motivated this). Mirrors rivet's darwin build: the base
# image bakes in osxcross + the MacOSX SDK (which provides CoreServices.h etc.
# that a plain zig cross-build lacks) AND Node 22 + corepack for our v8-bridge
# build.rs. Parameterized by TARGET so one file serves x64 + arm64.
#
#   TARGET = aarch64-apple-darwin | x86_64-apple-darwin
#   CLANG  = aarch64-apple-darwin20.4 | x86_64-apple-darwin20.4   (osxcross prefix)
FROM ghcr.io/rivet-dev/rivet/builder-base-osxcross:0e33ceb98

ARG TARGET=aarch64-apple-darwin
ARG CLANG=aarch64-apple-darwin20.4
ARG TRIGGER=branch

ENV SDK=/root/osxcross/target/SDK/MacOSX11.3.sdk \
    RUSTC_WRAPPER=

WORKDIR /build
COPY . .

# Use the repo-pinned toolchain (rust-toolchain.toml) + the darwin target.
RUN channel=$(awk -F'"' '/channel/ {print $2; exit}' rust-toolchain.toml) && \
    rustup toolchain install "$channel" --profile minimal && \
    rustup target add --toolchain "$channel" "$TARGET"

# The v8-runtime build.rs regenerates the V8 bridge via Node, so the pnpm
# workspace must be installed first. Exclude the leaf `website` package (pnpm
# 10.x symlink race), matching the native build.
RUN corepack enable && pnpm install --frozen-lockfile --filter='!@secure-exec/website'

RUN tu=$(echo "$TARGET" | tr 'a-z-' 'A-Z_') && \
    tl=$(echo "$TARGET" | tr - _) && \
    export BINDGEN_EXTRA_CLANG_ARGS_${tl}="--sysroot=$SDK -isystem $SDK/usr/include" && \
    export CFLAGS_${tl}="-B/root/osxcross/target/bin" && \
    export CXXFLAGS_${tl}="-B/root/osxcross/target/bin" && \
    export CC_${tl}=${CLANG}-clang && \
    export CXX_${tl}=${CLANG}-clang++ && \
    export AR_${tl}=${CLANG}-ar && \
    export RANLIB_${tl}=${CLANG}-ranlib && \
    export CARGO_TARGET_${tu}_LINKER=${CLANG}-clang && \
    if [ "$TRIGGER" = "release" ]; then FLAG="--release"; PROF=release; else FLAG=""; PROF=debug; fi && \
    cargo build $FLAG -p secure-exec-sidecar --target "$TARGET" && \
    mkdir -p /artifacts && \
    cp "target/$TARGET/$PROF/secure-exec-sidecar" /artifacts/secure-exec-sidecar

CMD ["ls", "-la", "/artifacts"]
