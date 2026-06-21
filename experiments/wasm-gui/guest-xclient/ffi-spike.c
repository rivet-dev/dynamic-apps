/* M8 libffi spike: prove a secure-exec-native DYNAMIC FFI CALL on wasm32-wasip1.
 *
 * wasm32-wasi has no libffi (no runtime trampolines / inline asm, the thing closures need), which is
 * what blocks the GTK/GObject stack (GObject's g_cclosure_marshal_generic calls handlers via ffi_call).
 * But our guests run inside the V8 sidecar, and V8's WebAssembly reflection can call a guest function
 * by its indirect-function-table index with dynamically-typed args. The host import `host_net.ffi_call`
 * (node_import_cache.rs) exposes exactly that. This test calls three functions purely by POINTER +
 * a runtime-built arg list (no static call site for the callee signature), which is the core libffi
 * capability GObject needs. Run: host --exec --guest ffi-spike.wasm.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* arg/return kinds: 0=i32, 1=i64, 2=f32, 3=f64, 255=void. Each arg value occupies an 8-byte slot. */
extern int ffi_call(unsigned fn_index, unsigned ret_kind, unsigned nargs,
                    const unsigned char *arg_kinds, const void *arg_vals, void *ret)
    __attribute__((import_module("host_net"), import_name("ffi_call")));

__attribute__((noinline)) int add(int a, int b) { return a + b; }
__attribute__((noinline)) double dmul(double a, double b) { return a * b; }
__attribute__((noinline)) int slen(const char *s) { return (int) strlen(s); }

int main(void) {
    int pass = 1;

    /* (1) int add(int,int) — i32 args + i32 return, called only via its function pointer. */
    {
        unsigned idx = (unsigned) (uintptr_t) &add;   /* wasm: a function pointer IS its table index */
        unsigned char kinds[2] = { 0, 0 };
        unsigned char vals[16] = { 0 };
        *(int *) (vals + 0) = 7;
        *(int *) (vals + 8) = 5;
        int64_t r = 0;
        int rc = ffi_call(idx, 0, 2, kinds, vals, &r);
        printf("ffi_call add(7,5) rc=%d -> %d (want 12)\n", rc, (int) r);
        if (rc != 0 || (int) r != 12) pass = 0;
    }

    /* (2) double dmul(double,double) — f64 args + f64 return. */
    {
        unsigned idx = (unsigned) (uintptr_t) &dmul;
        unsigned char kinds[2] = { 3, 3 };
        unsigned char vals[16] = { 0 };
        *(double *) (vals + 0) = 3.0;
        *(double *) (vals + 8) = 4.0;
        double r = 0;
        int rc = ffi_call(idx, 3, 2, kinds, vals, &r);
        printf("ffi_call dmul(3,4) rc=%d -> %f (want 12)\n", rc, r);
        if (rc != 0 || r != 12.0) pass = 0;
    }

    /* (3) int slen(const char*) — pointer arg (i32 on wasm32) + i32 return. */
    {
        const char *msg = "hello";
        unsigned idx = (unsigned) (uintptr_t) &slen;
        unsigned char kinds[1] = { 0 };
        unsigned char vals[8] = { 0 };
        *(const char **) (vals + 0) = msg;
        int64_t r = 0;
        int rc = ffi_call(idx, 0, 1, kinds, vals, &r);
        printf("ffi_call slen(\"hello\") rc=%d -> %d (want 5)\n", rc, (int) r);
        if (rc != 0 || (int) r != 5) pass = 0;
    }

    printf("M8-FFI-SPIKE: %s\n", pass ? "PASS" : "FAIL");
    fflush(stdout);
    return pass ? 0 : 1;
}
