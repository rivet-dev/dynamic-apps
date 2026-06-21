/* libffi-wasm ABI test: exercise the REAL libffi public interface (ffi_prep_cif + ffi_call +
 * ffi_closure_alloc/ffi_prep_closure_loc) the way GObject's marshalling does, proving the shim works
 * end to end on wasm32-wasip1. Run: host --exec --guest ffi-abi-test.wasm.
 */
#include "ffi.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

static int add(int a, int b) { return a + b; }
static double dmul(double a, double b) { return a * b; }
static int slen(const char *s) { return (int) strlen(s); }

/* libffi-style closure callback: sums the two int args plus a captured base (user_data). */
static void sum_handler(ffi_cif *cif, void *ret, void **args, void *user_data) {
    (void) cif;
    int a = *(int *) args[0];
    int b = *(int *) args[1];
    int base = (int) (intptr_t) user_data;
    *(ffi_arg *) ret = (ffi_arg) (base + a + b);
}

int main(void) {
    int pass = 1;

    /* (1) ffi_call int add(int,int) through the real ABI. */
    {
        ffi_cif cif;
        ffi_type *at[2] = { &ffi_type_sint, &ffi_type_sint };
        if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 2, &ffi_type_sint, at) != FFI_OK) pass = 0;
        int a = 7, b = 5;
        void *av[2] = { &a, &b };
        ffi_arg r = 0;
        ffi_call(&cif, (void (*)(void)) add, &r, av);
        printf("ffi_call add(7,5) = %d (want 12)\n", (int) r);
        if ((int) r != 12) pass = 0;
    }

    /* (2) ffi_call double dmul(double,double). */
    {
        ffi_cif cif;
        ffi_type *at[2] = { &ffi_type_double, &ffi_type_double };
        ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 2, &ffi_type_double, at);
        double a = 3.0, b = 4.0, r = 0;
        void *av[2] = { &a, &b };
        ffi_call(&cif, (void (*)(void)) dmul, &r, av);
        printf("ffi_call dmul(3,4) = %f (want 12)\n", r);
        if (r != 12.0) pass = 0;
    }

    /* (3) ffi_call int slen(const char*) — pointer arg + int return. */
    {
        ffi_cif cif;
        ffi_type *at[1] = { &ffi_type_pointer };
        ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 1, &ffi_type_sint, at);
        const char *s = "hello";
        void *av[1] = { &s };
        ffi_arg r = 0;
        ffi_call(&cif, (void (*)(void)) slen, &r, av);
        printf("ffi_call slen(\"hello\") = %d (want 5)\n", (int) r);
        if ((int) r != 5) pass = 0;
    }

    /* (4) ffi_closure: build a callback at runtime that captures base=1000, call it as a normal fn. */
    {
        ffi_cif cif;
        ffi_type *at[2] = { &ffi_type_sint, &ffi_type_sint };
        ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 2, &ffi_type_sint, at);
        void *code = 0;
        ffi_closure *cl = (ffi_closure *) ffi_closure_alloc(sizeof(ffi_closure), &code);
        if (!cl || !code) { printf("ffi_closure_alloc failed\n"); pass = 0; }
        else {
            if (ffi_prep_closure_loc(cl, &cif, sum_handler, (void *) (intptr_t) 1000, code) != FFI_OK) pass = 0;
            int (*callback)(int, int) = (int (*)(int, int)) code;
            int r = callback(20, 3); /* -> sum_handler: 1000 + 20 + 3 = 1023 */
            printf("ffi_closure callback(20,3) = %d (want 1023)\n", r);
            if (r != 1023) pass = 0;
            ffi_closure_free(cl);
        }
    }

    printf("M8-FFI-ABI: %s\n", pass ? "PASS" : "FAIL");
    fflush(stdout);
    return pass ? 0 : 1;
}
