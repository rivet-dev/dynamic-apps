/* libffi-under-threads (WASM-THREADS-SPEC.md DoD R5/§9): concurrent host ffi_call (the GObject marshal
 * path) from multiple worker threads. Each worker is its own V8 isolate; ffi_call reflects on that
 * isolate's indirect function table. Proves dynamic FFI works correctly under concurrency. */
#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <stdatomic.h>
extern int ffi_call(unsigned fn_index, unsigned ret_kind, unsigned nargs,
                    const unsigned char *arg_kinds, const void *arg_vals, void *ret)
    __attribute__((import_module("host_net"), import_name("ffi_call")));
__attribute__((noinline)) int add(int a, int b) { return a + b; }
static atomic_int ok = 0;
#define N 4
#define ITERS 500
static void *worker(void *arg) {
  (void) arg;
  unsigned idx = (unsigned) (uintptr_t) &add;
  for (int i = 0; i < ITERS; i++) {
    unsigned char kinds[2] = { 0, 0 };
    unsigned char vals[16] = { 0 };
    *(int *) (vals + 0) = i;
    *(int *) (vals + 8) = i + 1;
    int64_t r = 0;
    if (ffi_call(idx, 0, 2, kinds, vals, &r) == 0 && (int) r == (i + i + 1))
      atomic_fetch_add_explicit(&ok, 1, memory_order_relaxed);
  }
  return NULL;
}
int main(void) {
  pthread_t t[N];
  for (int i = 0; i < N; i++) pthread_create(&t[i], NULL, worker, NULL);
  for (int i = 0; i < N; i++) pthread_join(t[i], NULL);
  int got = atomic_load(&ok);
  printf("M8-THREADS-FFI: ok=%d/%d\n", got, N * ITERS);
  printf("M8-THREADS-FFI: %s\n", got == N * ITERS ? "PASS" : "FAIL");
  return 0;
}
