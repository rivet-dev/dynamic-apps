/* POSIX: an empty path is ENOENT. wasi-libc instead resolves open("")/stat("")/lstat("") against the cwd
 * to the DIRECTORY itself, so the empty path wrongly succeeds. Concretely Thunar's
 * gtk_image_new_from_file("") -> gdk_pixbuf_new_from_file("") -> fopen("") gets an fd to the cwd directory,
 * then gdk_pixbuf hangs parsing a directory as an image -> the ThunarWindow constructor never returns ->
 * no window (the XU5 block).
 *
 * Glibc rejects an empty path with ENOENT in the libc before the syscall; wasi-libc does not. This shim
 * restores that POSIX behavior at the libc boundary by wrapping the entry points (link with
 * -Wl,--wrap=open,--wrap=openat,--wrap=fopen,--wrap=stat,--wrap=lstat). Toolchain --wrap shim
 * (constraint #5), no component patched. */
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

extern int   __real_open(const char *path, int flags, ...);
extern int   __real_openat(int dirfd, const char *path, int flags, ...);
extern FILE *__real_fopen(const char *path, const char *mode);
extern int   __real_stat(const char *path, struct stat *st);
extern int   __real_lstat(const char *path, struct stat *st);

static int empty(const char *p) { return p != NULL && p[0] == '\0'; }

int __wrap_open(const char *path, int flags, ...) {
  if (empty(path)) { errno = ENOENT; return -1; }
  mode_t mode = 0;
  if (flags & O_CREAT) { va_list ap; va_start(ap, flags); mode = (mode_t) va_arg(ap, int); va_end(ap); }
  return __real_open(path, flags, mode);
}

int __wrap_openat(int dirfd, const char *path, int flags, ...) {
  if (empty(path)) { errno = ENOENT; return -1; }
  mode_t mode = 0;
  if (flags & O_CREAT) { va_list ap; va_start(ap, flags); mode = (mode_t) va_arg(ap, int); va_end(ap); }
  return __real_openat(dirfd, path, flags, mode);
}

FILE *__wrap_fopen(const char *path, const char *mode) {
  if (empty(path)) { errno = ENOENT; return NULL; }
  return __real_fopen(path, mode);
}

int __wrap_stat(const char *path, struct stat *st) {
  if (empty(path)) { errno = ENOENT; return -1; }
  return __real_stat(path, st);
}

int __wrap_lstat(const char *path, struct stat *st) {
  if (empty(path)) { errno = ENOENT; return -1; }
  return __real_lstat(path, st);
}
