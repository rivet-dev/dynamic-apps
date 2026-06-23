/* Isolates the pcmanfm/FmJob blocker to the kernel primitive, no glib:
 * does a pipe write from a WORKER thread wake a poll() blocked in the MAIN thread?
 * This is exactly what glib's GWakeup (g_main_context_wakeup) relies on to interrupt the GTK main
 * loop's poll when an async job posts a result. Raw pthreads + pipe + poll.
 *
 * PASS  = main's poll wakes PROMPTLY (<2000ms) on the worker's write.
 * FAIL  = poll only returns at its 5000ms timeout -> cross-thread pipe->poll wakeup is broken.
 */
#include <pthread.h>
#include <poll.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>

static int pfd[2];

static void *worker(void *arg) {
  (void) arg;
  struct timespec ts = {0, 200 * 1000 * 1000};
  nanosleep(&ts, NULL);
  fprintf(stderr, "PPW: worker writing wake byte to fd %d\n", pfd[1]); fflush(stderr);
  char b = 1;
  ssize_t n = write(pfd[1], &b, 1);
  fprintf(stderr, "PPW: worker wrote %zd\n", n); fflush(stderr);
  return NULL;
}

int main(void) {
  if (pipe(pfd) != 0) { printf("POLL-PIPE-WAKE: FAIL (pipe() failed)\n"); return 1; }
  pthread_t t;
  pthread_create(&t, NULL, worker, NULL);
  struct pollfd p = { .fd = pfd[0], .events = POLLIN };
  fprintf(stderr, "PPW: main poll(timeout=5000) on pipe read fd %d\n", pfd[0]); fflush(stderr);
  struct timespec t0, t1;
  clock_gettime(CLOCK_MONOTONIC, &t0);
  int r = poll(&p, 1, 5000);
  clock_gettime(CLOCK_MONOTONIC, &t1);
  long ms = (t1.tv_sec - t0.tv_sec) * 1000 + (t1.tv_nsec - t0.tv_nsec) / 1000000;
  pthread_join(t, NULL);
  fprintf(stderr, "PPW: poll returned r=%d revents=0x%x after %ldms\n", r, p.revents, ms); fflush(stderr);
  int pass = (r == 1) && (p.revents & POLLIN) && (ms < 2000);
  printf("POLL-PIPE-WAKE: r=%d revents=0x%x ms=%ld\n", r, p.revents, ms);
  printf("POLL-PIPE-WAKE: %s\n", pass ? "PASS" : "FAIL");
  return 0;
}
