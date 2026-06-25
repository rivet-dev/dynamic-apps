/* XU5 targeted repro: does a worker thread doing HOST SYNC-RPCs (file I/O) complete while the MAIN thread
 * blocks WITHOUT making its own sync-RPC? The 3 prior repros passed but their workers did pure computation
 * (no host sync-RPC) -- Thunar's construction workers do file/icon I/O while the main thread blocks, and
 * only a g_printerr (a host write sync-RPC) unblocks it. Tests:
 *   A: worker does N file-I/O sync-RPCs, then exits; main g_thread_join (blocking, NO I/O of its own).
 *   B: main pushes a thread-default ctx + spawns a worker that does file I/O then g_main_context_invoke()s
 *      the pushed ctx; main blocks in `while(!done) g_main_context_iteration(ctx, TRUE)` (NO other I/O).
 * If either hangs, that is the runtime pattern behind the Thunar window hang. */
#include <glib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

static volatile int a_reads = 0;

static gpointer worker_fileio(gpointer d) {
  (void) d;
  for (int i = 0; i < 100; i++) {
    int fd = open("/etc/machine-id", O_RDONLY);
    if (fd >= 0) { char buf[64]; (void) !read(fd, buf, sizeof buf); close(fd); a_reads++; }
  }
  fprintf(stderr, "SRT: worker_fileio did %d reads, exiting\n", a_reads);
  return GINT_TO_POINTER(1);
}

static volatile int b_done = 0;
static GMainContext *b_ctx;
static gboolean b_invoke_cb(gpointer d) { (void) d; fprintf(stderr, "SRT-B: invoke ran ON MAIN -> done\n"); b_done = 1; return G_SOURCE_REMOVE; }
static gpointer worker_invoke(gpointer d) {
  (void) d;
  for (int i = 0; i < 50; i++) { int fd = open("/etc/machine-id", O_RDONLY); if (fd>=0){char b[64];(void)!read(fd,b,sizeof b);close(fd);} }
  fprintf(stderr, "SRT-B: worker did file I/O, posting g_main_context_invoke to the pushed ctx\n");
  g_main_context_invoke(b_ctx, b_invoke_cb, NULL);
  return NULL;
}

int main(void) {
  fprintf(stderr, "SRT: TEST A -- worker host file-I/O + main g_thread_join (no main I/O)...\n");
  GThread *t = g_thread_new("a", worker_fileio, NULL);
  g_thread_join(t);
  fprintf(stderr, "SRT: TEST A join returned (reads=%d) -> A PASS\n", a_reads);

  fprintf(stderr, "SRT: TEST B -- main pumps pushed ctx, worker file-I/O + g_main_context_invoke...\n");
  b_ctx = g_main_context_new();
  g_main_context_push_thread_default(b_ctx);
  GThread *t2 = g_thread_new("b", worker_invoke, NULL);
  int n = 0;
  while (!b_done && n++ < 2000) g_main_context_iteration(b_ctx, TRUE);
  g_thread_join(t2);
  g_main_context_pop_thread_default(b_ctx);
  fprintf(stderr, "SRT: TEST B done=%d iters=%d -> %s\n", b_done, n, b_done ? "B PASS" : "B FAIL (hang masked by timeout)");

  printf("SYNCRPC-THREAD: A=%s B=%s\n", a_reads == 100 ? "PASS" : "FAIL", b_done ? "PASS" : "FAIL");
  return 0;
}
