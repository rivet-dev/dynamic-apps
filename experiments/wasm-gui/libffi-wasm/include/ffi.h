/* libffi-wasm: a libffi-ABI-compatible shim for wasm32-wasip1 (secure-exec).
 *
 * Upstream libffi has no wasm port (no runtime trampolines / inline asm). This shim implements the
 * libffi public ABI that GObject/GLib use, backed by two secure-exec-native primitives proven in the
 * M8 spikes: (1) ffi_call via the `host_net.ffi_call` import (V8 reflection calls a guest function by
 * indirect-table index with marshalled args); (2) closures via a pre-generated trampoline pool (pure
 * wasm). This is a deliberately FOCUSED subset (the scalar/pointer types GObject's generic marshaller
 * uses); struct-by-value and variadic cif are not yet handled. It is enough to link GObject's closure
 * marshalling and prove the libffi interface works on wasm32-wasip1.
 */
#ifndef LIBFFI_WASM_FFI_H
#define LIBFFI_WASM_FFI_H

#include <stddef.h>
#include <stdint.h>
#include <ffitarget.h>

#ifdef __cplusplus
extern "C" {
#endif

/* libffi exposes ffi_arg / ffi_sarg as the natural word; wasm32 -> 32-bit (matches uintptr_t). */
typedef uintptr_t ffi_arg;
typedef intptr_t ffi_sarg;

typedef enum {
    FFI_OK = 0,
    FFI_BAD_TYPEDEF,
    FFI_BAD_ABI,
    FFI_BAD_ARGTYPE
} ffi_status;

/* Only one ABI on wasm. */
typedef enum { FFI_FIRST_ABI = 0, FFI_WASM32 = 1, FFI_DEFAULT_ABI = FFI_WASM32, FFI_LAST_ABI } ffi_abi;

/* ffi_type kinds (subset of upstream FFI_TYPE_*). */
#define FFI_TYPE_VOID 0
#define FFI_TYPE_INT 1
#define FFI_TYPE_FLOAT 2
#define FFI_TYPE_DOUBLE 3
#define FFI_TYPE_UINT8 5
#define FFI_TYPE_SINT8 6
#define FFI_TYPE_UINT16 7
#define FFI_TYPE_SINT16 8
#define FFI_TYPE_UINT32 9
#define FFI_TYPE_SINT32 10
#define FFI_TYPE_UINT64 11
#define FFI_TYPE_SINT64 12
#define FFI_TYPE_STRUCT 13
#define FFI_TYPE_POINTER 14

typedef struct _ffi_type {
    size_t size;
    unsigned short alignment;
    unsigned short type;
    struct _ffi_type **elements; /* for FFI_TYPE_STRUCT (unsupported here) */
} ffi_type;

extern ffi_type ffi_type_void;
extern ffi_type ffi_type_uint8;
extern ffi_type ffi_type_sint8;
extern ffi_type ffi_type_uint16;
extern ffi_type ffi_type_sint16;
extern ffi_type ffi_type_uint32;
extern ffi_type ffi_type_sint32;
extern ffi_type ffi_type_uint64;
extern ffi_type ffi_type_sint64;
extern ffi_type ffi_type_float;
extern ffi_type ffi_type_double;
extern ffi_type ffi_type_pointer;
/* Convenience aliases libffi users rely on (sizes match wasm32 LP32/ILP32-ish). */
#define ffi_type_uchar ffi_type_uint8
#define ffi_type_schar ffi_type_sint8
#define ffi_type_ushort ffi_type_uint16
#define ffi_type_sshort ffi_type_sint16
#define ffi_type_uint ffi_type_uint32
#define ffi_type_sint ffi_type_sint32
#define ffi_type_ulong ffi_type_uint32
#define ffi_type_slong ffi_type_sint32

typedef struct {
    ffi_abi abi;
    unsigned nargs;
    ffi_type **arg_types;
    ffi_type *rtype;
    unsigned bytes;
    unsigned flags;
} ffi_cif;

ffi_status ffi_prep_cif(ffi_cif *cif, ffi_abi abi, unsigned nargs, ffi_type *rtype, ffi_type **atypes);
ffi_status ffi_prep_cif_var(ffi_cif *cif, ffi_abi abi, unsigned nfixedargs, unsigned ntotalargs,
                            ffi_type *rtype, ffi_type **atypes);
void ffi_call(ffi_cif *cif, void (*fn)(void), void *rvalue, void **avalue);

/* Closures. ffi_closure_alloc returns the writable closure; *code receives the executable address
 * (the function pointer the caller invokes). On wasm both are the same trampoline pointer. */
typedef struct ffi_closure {
    ffi_cif *cif;
    void (*fun)(ffi_cif *, void *ret, void **args, void *user_data);
    void *user_data;
    int pool_slot; /* wasm-shim: which trampoline slot backs this closure (-1 if none) */
} ffi_closure;

void *ffi_closure_alloc(size_t size, void **code);
void ffi_closure_free(void *closure);
ffi_status ffi_prep_closure_loc(ffi_closure *closure, ffi_cif *cif,
                                void (*fun)(ffi_cif *, void *, void **, void *),
                                void *user_data, void *codeloc);

#ifdef __cplusplus
}
#endif

#endif /* LIBFFI_WASM_FFI_H */
