/* Compat <grp.h> for wasm32-wasip1: wasi-libc has no group database. GLib's GIO (glocalfileinfo.c)
 * resolves file group names via getgrgid()/getgrnam(); the sandbox has no /etc/group, so these are
 * inline stubs returning "not found". static inline keeps them link-conflict-free across TUs. */
#ifndef WASM_COMPAT_GRP_H
#define WASM_COMPAT_GRP_H

#include <sys/types.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct group {
    char *gr_name;
    char *gr_passwd;
    gid_t gr_gid;
    char **gr_mem;
};

static inline struct group *getgrgid(gid_t gid) { (void) gid; return 0; }
static inline struct group *getgrnam(const char *name) { (void) name; return 0; }
/* Group-database enumeration (openbox obt/paths.c find_uid_gid uses these). The sandbox has no
 * /etc/group, so enumeration is empty. Declaring them with correct signatures avoids the implicit-int
 * wasm-ABI mismatch that otherwise traps when the caller's expected return type differs from the stub. */
static inline struct group *getgrent(void) { return 0; }
static inline void setgrent(void) { }
static inline void endgrent(void) { }
static inline int getgrgid_r(gid_t gid, struct group *grp, char *buf, size_t buflen,
                             struct group **result) {
    (void) gid; (void) grp; (void) buf; (void) buflen;
    if (result) *result = 0;
    return 0;
}
static inline int getgrnam_r(const char *name, struct group *grp, char *buf, size_t buflen,
                             struct group **result) {
    (void) name; (void) grp; (void) buf; (void) buflen;
    if (result) *result = 0;
    return 0;
}

#ifdef __cplusplus
}
#endif

#endif /* WASM_COMPAT_GRP_H */
