/* Minimal reimplementation of the `libsha1` API the X server uses when built with
   HAVE_SHA1_IN_LIBSHA1 (os/xsha1.c calls sha1_begin/sha1_hash/sha1_end on a sha1_ctx).
   Public-domain SHA-1 (Steve Reid's implementation), wrapped in the libsha1 entry points.
   This exists only to satisfy the X server's content-digest needs inside the wasm sandbox. */
#ifndef LIBSHA1_H
#define LIBSHA1_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t state[5];
    uint64_t count;          /* total message length in bytes */
    unsigned char buffer[64];
} sha1_ctx;

void sha1_begin(sha1_ctx *ctx);
void sha1_hash(const void *data, size_t len, sha1_ctx *ctx);
void sha1_end(unsigned char digest[20], sha1_ctx *ctx);

#endif /* LIBSHA1_H */
