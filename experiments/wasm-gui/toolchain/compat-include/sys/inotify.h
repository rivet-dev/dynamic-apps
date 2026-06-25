/* sys/inotify.h for the wasm-gui toolchain.
 *
 * wasi has no inotify, so GLib's meson build does not detect sys/inotify.h and omits its inotify
 * file-monitor backend. With no supported local-file-monitor backend, g_file_monitor falls back to
 * GPollFileMonitor, which issues g_file_query_info_async per watched file -- and those async query_info
 * workers hang (task#11: the xfce4-panel applicationsmenu watches ~46 menu files via garcon's
 * g_file_monitor, producing 46 hung workers and a black panel).
 *
 * Providing this header makes meson compile GLib's UNMODIFIED inotify backend; the runtime stubs in
 * wasi-compat.c give it a no-op inotify (a never-ready fd, no events). GIO then uses the no-op inotify
 * monitor instead of the hanging poll monitor. A static desktop menu needs no live change events.
 * Constraint #5: this is a sysroot/runtime shim, not a component patch. */
#ifndef _SYS_INOTIFY_H
#define _SYS_INOTIFY_H 1

#include <stdint.h>
#include <sys/types.h>

struct inotify_event {
  int       wd;
  uint32_t  mask;
  uint32_t  cookie;
  uint32_t  len;
  char      name[];
};

/* flags for inotify_init1 */
#define IN_CLOEXEC   02000000
#define IN_NONBLOCK  00004000

/* events a watch can report */
#define IN_ACCESS        0x00000001
#define IN_MODIFY        0x00000002
#define IN_ATTRIB        0x00000004
#define IN_CLOSE_WRITE   0x00000008
#define IN_CLOSE_NOWRITE 0x00000010
#define IN_CLOSE         (IN_CLOSE_WRITE | IN_CLOSE_NOWRITE)
#define IN_OPEN          0x00000020
#define IN_MOVED_FROM    0x00000040
#define IN_MOVED_TO      0x00000080
#define IN_MOVE          (IN_MOVED_FROM | IN_MOVED_TO)
#define IN_CREATE        0x00000100
#define IN_DELETE        0x00000200
#define IN_DELETE_SELF   0x00000400
#define IN_MOVE_SELF     0x00000800

/* events sent by the kernel */
#define IN_UNMOUNT       0x00002000
#define IN_Q_OVERFLOW    0x00004000
#define IN_IGNORED       0x00008000

/* special flags */
#define IN_ONLYDIR       0x01000000
#define IN_DONT_FOLLOW   0x02000000
#define IN_EXCL_UNLINK   0x04000000
#define IN_MASK_CREATE   0x10000000
#define IN_MASK_ADD      0x20000000
#define IN_ISDIR         0x40000000
#define IN_ONESHOT       0x80000000

#define IN_ALL_EVENTS \
  (IN_ACCESS | IN_MODIFY | IN_ATTRIB | IN_CLOSE_WRITE | IN_CLOSE_NOWRITE | \
   IN_OPEN | IN_MOVED_FROM | IN_MOVED_TO | IN_CREATE | IN_DELETE | \
   IN_DELETE_SELF | IN_MOVE_SELF)

#ifdef __cplusplus
extern "C" {
#endif

int inotify_init (void);
int inotify_init1 (int __flags);
int inotify_add_watch (int __fd, const char *__name, uint32_t __mask);
int inotify_rm_watch (int __fd, int __wd);

#ifdef __cplusplus
}
#endif

#endif /* _SYS_INOTIFY_H */
