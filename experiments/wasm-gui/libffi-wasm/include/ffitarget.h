/* libffi-wasm: target definitions for wasm32-wasip1. Upstream libffi splits arch specifics into
 * <ffitarget.h>; consumers (and ffi.h) include it. Everything wasm needs lives in ffi.h, so this is
 * the minimal compatibility shim. */
#ifndef LIBFFI_WASM_FFITARGET_H
#define LIBFFI_WASM_FFITARGET_H

/* No closure trampoline code-size on wasm (closures are a pre-generated function-table pool, not
 * written machine code), but libffi consumers expect this macro to exist. */
#ifndef FFI_TRAMPOLINE_SIZE
#define FFI_TRAMPOLINE_SIZE 0
#endif

#ifndef FFI_NATIVE_RAW_API
#define FFI_NATIVE_RAW_API 0
#endif

#endif /* LIBFFI_WASM_FFITARGET_H */
