/* Compat <sys/shm.h> for wasm32-wasip1: SysV shared memory stubs (no IPC on wasi). Symbols defined in
 * toolchain/wasi-compat.c; all fail so Cairo/X clients fall back to non-shared-memory image transport. */
#ifndef WASM_COMPAT_SYS_SHM_H
#define WASM_COMPAT_SYS_SHM_H
#include <sys/ipc.h>
#include <sys/types.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef unsigned long shmatt_t;
struct shmid_ds { struct ipc_perm shm_perm; unsigned long shm_segsz; shmatt_t shm_nattch; };
int shmget(key_t key, unsigned long size, int shmflg);
void *shmat(int shmid, const void *shmaddr, int shmflg);
int shmdt(const void *shmaddr);
int shmctl(int shmid, int cmd, struct shmid_ds *buf);
#ifdef __cplusplus
}
#endif
#endif
