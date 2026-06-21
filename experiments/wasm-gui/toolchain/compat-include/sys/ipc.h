/* Compat <sys/ipc.h> for wasm32-wasip1: wasi has no SysV IPC. Cairo's Xlib XShm backend includes it;
 * provide the types/macros so it compiles (XShm fails at runtime -> cairo falls back to XPutImage). */
#ifndef WASM_COMPAT_SYS_IPC_H
#define WASM_COMPAT_SYS_IPC_H
#include <sys/types.h>
#ifndef IPC_PRIVATE
#define IPC_PRIVATE 0
#define IPC_CREAT  01000
#define IPC_EXCL   02000
#define IPC_NOWAIT 04000
#define IPC_RMID 0
#define IPC_SET  1
#define IPC_STAT 2
#endif
#ifndef __key_t_defined
typedef int key_t;
#define __key_t_defined
#endif
struct ipc_perm { key_t __key; unsigned uid, gid, cuid, cgid, mode; unsigned short __seq; };
#endif
