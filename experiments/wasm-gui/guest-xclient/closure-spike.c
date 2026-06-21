/* M8 libffi spike, part 2: ffi_closure on wasm32-wasip1 via the TRAMPOLINE-POOL technique (pure wasm,
 * NO host import, NO V8 engine flag). libffi closures are the other thing GObject needs (signal
 * handlers / vfuncs): a function pointer that, when called, runs a generic handler with captured data.
 * wasm guests can't synthesize functions at runtime, BUT a function pointer can be minted from a
 * PRE-GENERATED pool of trampolines: each pool slot is a distinct wasm function (one per slot/signature)
 * that forwards to a generic dispatcher with its slot's captured data. `ffi_prep_closure` hands out a
 * free slot's trampoline as the closure's function pointer. This is exactly how a wasm libffi shim
 * implements closures without runtime code generation (bounded to the signatures whose trampolines are
 * pre-generated; a full shim generates a pool per signature class GObject uses).
 *
 * The alternative (a single generic closure of ANY signature) needs V8's `WebAssembly.Function` type
 * reflection, which is an engine flag (--experimental-wasm-type-reflection) and is intentionally NOT
 * enabled here to keep this experiment isolated from the core v8-runtime. The pool below needs none of
 * that. Run: host --exec --guest closure-spike.wasm. Build with --export-table, no --fpcast-emu.
 */
#include <stdio.h>
#include <stdint.h>

/* Generic closure dispatcher: a real shim unpacks args per the ffi_cif and looks up the captured
 * user-data by slot; here we just prove the captured id + arg flow through correctly. */
__attribute__((noinline)) int closure_dispatch(int captured_id, int x) {
    return captured_id + x;
}

#define POOL 8
static int pool_id[POOL];
static int pool_used[POOL];

/* One trampoline per pool slot (signature (i32)->i32), each forwarding to the dispatcher with ITS
 * slot's captured id. Address-taken below, so each lands in the indirect function table. */
#define TRAMP(k) static int tramp_##k(int x) { return closure_dispatch(pool_id[k], x); }
TRAMP(0) TRAMP(1) TRAMP(2) TRAMP(3) TRAMP(4) TRAMP(5) TRAMP(6) TRAMP(7)

typedef int (*int_fn)(int);
static const int_fn tramps[POOL] = {
    tramp_0, tramp_1, tramp_2, tramp_3, tramp_4, tramp_5, tramp_6, tramp_7,
};

/* ffi_prep_closure analogue: capture `id` in a free slot, return that slot's trampoline as a usable
 * function pointer the caller invokes like any other (i32)->i32 callback. */
static int_fn closure_alloc(int id) {
    for (int i = 0; i < POOL; i++) {
        if (!pool_used[i]) { pool_used[i] = 1; pool_id[i] = id; return tramps[i]; }
    }
    return 0;
}

int main(void) {
    int pass = 1;

    int_fn c1 = closure_alloc(100);
    int_fn c2 = closure_alloc(1000);
    if (!c1 || !c2) { printf("M8-CLOSURE-SPIKE: FAIL (pool exhausted)\n"); fflush(stdout); return 1; }

    int r1 = c1(7);   /* call through the minted funcref -> closure_dispatch(100, 7)  = 107 */
    int r2 = c2(7);   /* a second, distinct closure        -> closure_dispatch(1000, 7) = 1007 */
    printf("closure1(7) -> %d (want 107)\n", r1);
    printf("closure2(7) -> %d (want 1007)\n", r2);
    if (r1 != 107 || r2 != 1007) pass = 0;

    /* Distinct closures must NOT alias (each keeps its own captured id). */
    if (c1 == c2) pass = 0;

    printf("M8-CLOSURE-SPIKE: %s\n", pass ? "PASS" : "FAIL");
    fflush(stdout);
    return pass ? 0 : 1;
}
