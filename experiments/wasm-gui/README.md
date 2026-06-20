# wasm-gui — a GUI rendered by wasm running inside secure-exec

A spike toward a full Linux GUI desktop where the GUI software is **cross-compiled to
`wasm32-wasip1`** (our toolchain) and **executed inside the real secure-exec V8 sidecar**, with a
native host (built on the standard secure-exec **Rust** client) reading the rendered framebuffer
and blitting it to a native surface. See `SPEC.md` and `../../WASM-GUI-DESKTOP-RESEARCH.md`.

**Strict constraint (SPEC §1a.2):** the process that executes *and* renders the guest is a native
app on `crates/secure-exec-client`, driving the guest through the real V8 sidecar. No wasmer, no
node:wasi, no TypeScript client, no `Command::new` in the execution/render path — the only process
spawn is the sidecar itself, done by the Rust client's transport.

```
guest/        Rust -> wasm32-wasip1 software renderer + frame protocol v0 (standalone wasm workspace)
host/         native Rust app on secure-exec-client: runs guest.wasm IN the sidecar, blits frames
              (member of the repo root workspace, shares Cargo.lock with the sidecar)
tests/        automated headless verification THROUGH secure-exec (build, run, golden pixels, PNG)
host-node/    SUPERSEDED node:wasi spike (determinism evidence only; not the product path)
```

## Run the automated test (headless — no display needed)

```sh
cd experiments/wasm-gui
./tests/run.sh
```
This builds the guest, builds the host + sidecar in the repo root workspace, runs `guest.wasm`
**inside the secure-exec V8 sidecar** via the Rust client, reads the framebuffer back through the
client (chunked PREAD), validates the frame header, checks golden pixels, and writes a PNG proof to
`~/tmp/gui-progress/assets/frame_secureexec.png`. Re-bless the golden after an intentional renderer
change with `./tests/run.sh --bless`.

Requirements: `cargo` + the `wasm32-wasip1` target (`rustup target add wasm32-wasip1`), `node`
(only for the pixel checker), and `ffmpeg` (PNG proof only). The sidecar is built from this repo.

## Run capture directly (what the test does)

```sh
# from repo root, after `cargo build -p secure-exec-sidecar -p wasm-gui-host`
target/debug/wasm-gui-host \
  --capture /tmp/frame.bin \
  --guest experiments/wasm-gui/guest/target/wasm32-wasip1/release/guest.wasm \
  --sidecar target/debug/secure-exec-sidecar
```

## Run the interactive window demo (on a machine WITH a display)

```sh
cd experiments/wasm-gui/guest && cargo build --target wasm32-wasip1 --release && cd -
cargo build -p wasm-gui-host --features window
target/debug/wasm-gui-host --window \
  --guest experiments/wasm-gui/guest/target/wasm32-wasip1/release/guest.wasm \
  --sidecar target/debug/secure-exec-sidecar
```
Opens a real OS window. The guest runs in `--loop` mode *inside the sidecar*; the host streams its
stdout frames (`ProcessOutputEvent`) through the client, blits them, and forwards mouse/keyboard
back via `WriteStdin`. This dev box is headless, so the window can't open here; the PNG proof shows
the identical frame.

## Run the real X11 stack (M4 / M4b / M5) — a wasm X server + wasm X clients

The frontier milestones cross-compile the **real X.Org `Xvfb` X server** and X **clients** from source to
`wasm32-wasip1` and run them **as guests inside secure-exec**, talking X11 over an AF_UNIX socket in the
kernel socket table. Build the pieces, then run:

```sh
# 1. Build the sidecar + host (repo root)
cargo build -p secure-exec-sidecar -p wasm-gui-host

# 2. Build the raw-X11 client (the X server, Xvfb.wasm, is built by scripts/link-xvfb.sh +
#    a `wasm-opt --fpcast-emu` pass — see SPEC.md for the full server build).
experiments/wasm-gui/scripts/build-xfill.sh

# 3a. M4b — one X client draws on the wasm X server; read the framebuffer back:
target/debug/wasm-gui-host --xdemo --timeout 16 \
  --server experiments/wasm-gui/Xvfb.wasm \
  --client experiments/wasm-gui/guest-xclient/xfill.wasm \
  --fb-out /tmp/frame.bin --sidecar target/debug/secure-exec-sidecar \
  -- :0 -screen 0 640x480x24 -nolisten tcp -nolock -listen local -fbdir /data

# 3c. M5 — the standard window manager twm decorating a real libX11 client window:
#     (build the toolkit stack + twm + xwin first; see SPEC.md §M5 for the build commands)
target/debug/wasm-gui-host --xdemo --timeout 30 \
  --server experiments/wasm-gui/Xvfb.wasm \
  --client experiments/wasm-gui/twm.wasm \
  --client experiments/wasm-gui/guest-xclient/xwin.wasm \
  --fb-out /tmp/frame.bin --sidecar target/debug/secure-exec-sidecar \
  -- :0 -screen 0 640x480x24 -nolisten tcp -nolock -listen local -noreset -fbdir /data
# Automated: scripts/test-m5-twm.sh  (asserts twm decorates the window with a title bar)

# 3b. M5 — THREE clients (orange bg, green + blue rects) composited by one X server:
target/debug/wasm-gui-host --xdemo --timeout 22 \
  --server experiments/wasm-gui/Xvfb.wasm \
  --client "experiments/wasm-gui/guest-xclient/xfill.wasm 0 0 -1 -1 16746496" \
  --client "experiments/wasm-gui/guest-xclient/xfill.wasm 80 80 200 150 65280" \
  --client "experiments/wasm-gui/guest-xclient/xfill.wasm 360 250 200 150 255" \
  --fb-out /tmp/frame.bin --sidecar target/debug/secure-exec-sidecar \
  -- :0 -screen 0 640x480x24 -nolisten tcp -nolock -listen local -noreset -fbdir /data
```

`/tmp/frame.bin` is an XWD dump (160-byte header + BGRX pixels) of the X server's root window, read out
of the VM via the kernel VFS. Automated equivalents: `scripts/test-m4b.sh` and
`scripts/test-m5-multiclient.sh` (both assert the framebuffer contains the clients' fill colors).
`--xdemo` runs the X server and every `--client` as **separate guest processes in one VM**, launching the
clients once the server reaches its dispatch loop, then reads the framebuffer back.

## How it works (the data path)

```
guest.wasm (wasm32-wasip1)  ──run inside──▶  secure-exec V8 sidecar
   software-renders RGBA frames                (real runtime; loads the module from the
   frame protocol v0 over WASI fds              trusted-client host entrypoint path)
        │ frames out (PREAD / stdout events)        ▲ input in (WriteStdin)
        ▼                                           │
   native host on secure-exec-client (Rust): reads frames, blits to winit/softbuffer, sends input
```

Frame protocol v0: `b"SXFB" | u32 LE width | u32 LE height`, then `width*height*4` bytes row-major
`[R,G,B,A]`. Capture mode (`guest --out <path>`) writes one deterministic frame to the guest VFS;
window mode (`guest --loop`) streams frames on stdout and reads input tokens on stdin.

## Scope / honesty

- The renderer is **hand-rolled**, not a real toolkit — it exercises the data path and the test
  harness. Real toolkits (FLTK/Tk, then an X server + WM) are milestones M3+ in `SPEC.md`.
- The guest frame produced through secure-exec is **byte-identical** (SHA-256) to the superseded
  node:wasi/wasmer spike output, which is how we know the V8 execution is faithful.
