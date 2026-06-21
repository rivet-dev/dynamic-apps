/* Compat <pwd.h> for wasm32-wasip1: the threaded sysroot has no pwd.h, but GLib gutils.c includes it
 * when meson detects getpwuid (wasi-compat provides a weak getpwuid). Provide struct passwd + the
 * declarations so it COMPILES; runtime values come from wasi-compat's stubs. */
#ifndef WASM_COMPAT_PWD_H
#define WASM_COMPAT_PWD_H
#if defined(__has_include) && __has_include_next(<pwd.h>)
#include_next <pwd.h>
#else
#include <sys/types.h>
#include <stddef.h>
struct passwd {
  char *pw_name;
  char *pw_passwd;
  uid_t pw_uid;
  gid_t pw_gid;
  /* Legacy BSD fields libX11 GetDflt.c references (XGetDefault home-dir lookup). Unused at runtime
   * (no passwd db in the sandbox); present so the struct member access compiles. */
  char *pw_age;
  char *pw_comment;
  char *pw_gecos;
  char *pw_dir;
  char *pw_shell;
};
struct passwd *getpwuid(uid_t);
struct passwd *getpwnam(const char *);
int getpwuid_r(uid_t, struct passwd *, char *, size_t, struct passwd **);
int getpwnam_r(const char *, struct passwd *, char *, size_t, struct passwd **);
#endif
#endif /* WASM_COMPAT_PWD_H */
