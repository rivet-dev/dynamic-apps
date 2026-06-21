/* libffi-wasm implementation: the libffi public ABI on wasm32-wasip1, backed by the two secure-exec
 * primitives proven in the M8 spikes. ffi_call -> host_net.ffi_call (V8 reflection). Closures -> a
 * pre-generated trampoline pool (pure wasm). FOCUSED subset: scalar + pointer arg/return types (what
 * GObject's generic marshaller uses); no struct-by-value, no variadic promotion. Closures here use the
 * canonical (word,word)->word signature (the common GObject signal-handler shape: instance + data);
 * a fuller shim generates a trampoline pool per signature class.
 */
#include "ffi.h"
#include <stdlib.h>
#include <string.h>

/* The proven host import: call guest function `fn_index` (an indirect-table index) with marshalled
 * args. kinds: 0=i32, 1=i64, 2=f32, 3=f64; ret kind also 255=void. 8-byte slot per arg / for the ret. */
extern int __se_ffi_call(unsigned fn_index, unsigned ret_kind, unsigned nargs,
                         const unsigned char *arg_kinds, const void *arg_vals, void *ret)
    __attribute__((import_module("host_net"), import_name("ffi_call")));

#define DEF_TYPE(name, sz, al, ty) ffi_type name = { (sz), (al), (ty), 0 }
DEF_TYPE(ffi_type_void, 1, 1, FFI_TYPE_VOID);
DEF_TYPE(ffi_type_uint8, 1, 1, FFI_TYPE_UINT8);
DEF_TYPE(ffi_type_sint8, 1, 1, FFI_TYPE_SINT8);
DEF_TYPE(ffi_type_uint16, 2, 2, FFI_TYPE_UINT16);
DEF_TYPE(ffi_type_sint16, 2, 2, FFI_TYPE_SINT16);
DEF_TYPE(ffi_type_uint32, 4, 4, FFI_TYPE_UINT32);
DEF_TYPE(ffi_type_sint32, 4, 4, FFI_TYPE_SINT32);
DEF_TYPE(ffi_type_uint64, 8, 8, FFI_TYPE_UINT64);
DEF_TYPE(ffi_type_sint64, 8, 8, FFI_TYPE_SINT64);
DEF_TYPE(ffi_type_float, 4, 4, FFI_TYPE_FLOAT);
DEF_TYPE(ffi_type_double, 8, 8, FFI_TYPE_DOUBLE);
DEF_TYPE(ffi_type_pointer, 4, 4, FFI_TYPE_POINTER);

ffi_status ffi_prep_cif(ffi_cif *cif, ffi_abi abi, unsigned nargs, ffi_type *rtype, ffi_type **atypes) {
    if (!cif || !rtype) return FFI_BAD_TYPEDEF;
    cif->abi = abi;
    cif->nargs = nargs;
    cif->rtype = rtype;
    cif->arg_types = atypes;
    cif->bytes = 0;
    cif->flags = 0;
    return FFI_OK;
}

ffi_status ffi_prep_cif_var(ffi_cif *cif, ffi_abi abi, unsigned nfixed, unsigned ntotal,
                            ffi_type *rtype, ffi_type **atypes) {
    (void) nfixed;
    return ffi_prep_cif(cif, abi, ntotal, rtype, atypes);
}

/* Classify an ffi_type to the wasm value kind the host import expects. */
static unsigned wasm_kind(const ffi_type *t) {
    switch (t->type) {
        case FFI_TYPE_VOID: return 255;
        case FFI_TYPE_FLOAT: return 2;
        case FFI_TYPE_DOUBLE: return 3;
        case FFI_TYPE_UINT64:
        case FFI_TYPE_SINT64: return 1;
        default: return 0; /* pointer + all <=32-bit integers pass as i32 on wasm32 */
    }
}

void ffi_call(ffi_cif *cif, void (*fn)(void), void *rvalue, void **avalue) {
    unsigned n = cif->nargs;
    unsigned char kinds[32];
    unsigned char vals[32 * 8];
    if (n > 32) return;
    for (unsigned i = 0; i < n; i++) {
        const ffi_type *t = cif->arg_types[i];
        kinds[i] = (unsigned char) wasm_kind(t);
        void *slot = vals + i * 8;
        memset(slot, 0, 8);
        switch (t->type) {
            case FFI_TYPE_POINTER: *(int32_t *) slot = (int32_t) (intptr_t) (*(void **) avalue[i]); break;
            case FFI_TYPE_UINT8:  *(int32_t *) slot = (int32_t) (*(uint8_t *) avalue[i]); break;
            case FFI_TYPE_SINT8:  *(int32_t *) slot = (int32_t) (*(int8_t *) avalue[i]); break;
            case FFI_TYPE_UINT16: *(int32_t *) slot = (int32_t) (*(uint16_t *) avalue[i]); break;
            case FFI_TYPE_SINT16: *(int32_t *) slot = (int32_t) (*(int16_t *) avalue[i]); break;
            case FFI_TYPE_UINT32:
            case FFI_TYPE_SINT32:
            case FFI_TYPE_INT:    *(int32_t *) slot = *(int32_t *) avalue[i]; break;
            case FFI_TYPE_UINT64:
            case FFI_TYPE_SINT64: *(int64_t *) slot = *(int64_t *) avalue[i]; break;
            case FFI_TYPE_FLOAT:  *(float *) slot = *(float *) avalue[i]; break;
            case FFI_TYPE_DOUBLE: *(double *) slot = *(double *) avalue[i]; break;
            default:              *(int32_t *) slot = *(int32_t *) avalue[i]; break;
        }
    }
    unsigned char retbuf[8];
    memset(retbuf, 0, 8);
    unsigned idx = (unsigned) (uintptr_t) fn; /* wasm: function pointer == indirect-table index */
    __se_ffi_call(idx, wasm_kind(cif->rtype), n, kinds, vals, retbuf);
    if (rvalue && cif->rtype->type != FFI_TYPE_VOID) {
        switch (cif->rtype->type) {
            case FFI_TYPE_FLOAT:  *(float *) rvalue = *(float *) retbuf; break;
            case FFI_TYPE_DOUBLE: *(double *) rvalue = *(double *) retbuf; break;
            case FFI_TYPE_UINT64:
            case FFI_TYPE_SINT64: *(int64_t *) rvalue = *(int64_t *) retbuf; break;
            case FFI_TYPE_POINTER: *(void **) rvalue = (void *) (intptr_t) (*(int32_t *) retbuf); break;
            default: *(ffi_arg *) rvalue = (ffi_arg) (*(int32_t *) retbuf); break; /* libffi widens */
        }
    }
}

/* ---- Closures: a pre-generated trampoline pool (pure wasm). ----
 * Canonical signature (word,word)->word. Each slot's trampoline forwards to the generic dispatcher,
 * which reconstructs avalue from the actual args and calls the user fun, libffi-style. */
#define FFI_POOL 16
static ffi_closure *g_slot[FFI_POOL];
static int g_used[FFI_POOL];

static intptr_t generic_dispatch(int k, intptr_t a0, intptr_t a1) {
    ffi_closure *c = g_slot[k];
    if (!c) return 0;
    void *av[2];
    av[0] = &a0;
    av[1] = &a1;
    intptr_t ret = 0;
    c->fun(c->cif, &ret, av, c->user_data);
    return ret;
}

#define TRAMP(k) static intptr_t tramp_##k(intptr_t a0, intptr_t a1) { return generic_dispatch(k, a0, a1); }
TRAMP(0) TRAMP(1) TRAMP(2) TRAMP(3) TRAMP(4) TRAMP(5) TRAMP(6) TRAMP(7)
TRAMP(8) TRAMP(9) TRAMP(10) TRAMP(11) TRAMP(12) TRAMP(13) TRAMP(14) TRAMP(15)

typedef intptr_t (*tramp_fn)(intptr_t, intptr_t);
static const tramp_fn g_tramps[FFI_POOL] = {
    tramp_0, tramp_1, tramp_2, tramp_3, tramp_4, tramp_5, tramp_6, tramp_7,
    tramp_8, tramp_9, tramp_10, tramp_11, tramp_12, tramp_13, tramp_14, tramp_15,
};

void *ffi_closure_alloc(size_t size, void **code) {
    if (size < sizeof(ffi_closure)) size = sizeof(ffi_closure);
    ffi_closure *c = (ffi_closure *) calloc(1, size);
    if (!c) { if (code) *code = 0; return 0; }
    c->pool_slot = -1;
    for (int i = 0; i < FFI_POOL; i++) {
        if (!g_used[i]) { g_used[i] = 1; c->pool_slot = i; g_slot[i] = c; break; }
    }
    if (c->pool_slot < 0) { free(c); if (code) *code = 0; return 0; }
    if (code) *code = (void *) g_tramps[c->pool_slot]; /* the callable the user invokes */
    return c;
}

void ffi_closure_free(void *closure) {
    ffi_closure *c = (ffi_closure *) closure;
    if (!c) return;
    if (c->pool_slot >= 0 && c->pool_slot < FFI_POOL) { g_used[c->pool_slot] = 0; g_slot[c->pool_slot] = 0; }
    free(c);
}

ffi_status ffi_prep_closure_loc(ffi_closure *closure, ffi_cif *cif,
                                void (*fun)(ffi_cif *, void *, void **, void *),
                                void *user_data, void *codeloc) {
    (void) codeloc;
    if (!closure || !cif || !fun) return FFI_BAD_TYPEDEF;
    closure->cif = cif;
    closure->fun = fun;
    closure->user_data = user_data;
    return FFI_OK;
}
