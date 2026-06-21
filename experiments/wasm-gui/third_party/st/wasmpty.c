/* st wasm PTY backend. Replaces st's forkpty/openpty + select/read/write over a libc fd with the
 * secure-exec kernel-PTY primitive: host_net.pty_spawn opens a real kernel PTY and launches the child
 * shell (a wasm guest) on the slave end, returning the master fd; pty_read/pty_write drive that master
 * (non-blocking). This is the same path proven by experiments/wasm-gui/scripts/test-m6-3-pty.sh, just
 * driving a real terminal emulator instead of a test harness. See M6.3-FINDINGS.md. */
#include <string.h>

extern unsigned pty_spawn(const char *cmd, unsigned cmd_len,
                          const char *argv_json, unsigned argv_json_len,
                          unsigned *ret_master_fd)
    __attribute__((import_module("host_net"), import_name("pty_spawn")));
extern unsigned pty_read(unsigned master_fd, char *buf, unsigned buf_len, unsigned *ret_read)
    __attribute__((import_module("host_net"), import_name("pty_read")));
extern unsigned pty_write(unsigned master_fd, const char *buf, unsigned buf_len,
                          unsigned *ret_written)
    __attribute__((import_module("host_net"), import_name("pty_write")));

/* wasi has no termios; st references tcsendbreak only via the (unused) -l serial path. Stub it so the
 * linker resolves it instead of emitting an unsatisfiable "env" import that breaks instantiation. */
int tcsendbreak(int fd, int duration) { (void) fd; (void) duration; return 0; }

static unsigned stwasm_master;
static int stwasm_have_master;

/* Spawn the child shell over a fresh kernel PTY; returns a sentinel fd (the master) or -1. */
int stwasm_spawn(const char *cmd) {
	unsigned m = 0;
	if (pty_spawn(cmd, (unsigned) strlen(cmd), "[]", 2, &m) != 0)
		return -1;
	stwasm_master = m;
	stwasm_have_master = 1;
	return (int) m;
}

/* Non-blocking read from the PTY master. Returns bytes read (0 = nothing available right now), -1 on
 * error. Never blocks: st's poll loop calls this each tick. */
long stwasm_read(char *buf, unsigned len) {
	unsigned got = 0;
	if (!stwasm_have_master)
		return -1;
	if (pty_read(stwasm_master, buf, len, &got) != 0)
		return -1;
	return (long) got;
}

/* Write to the PTY master (the user's keystrokes). Returns bytes written or -1. */
long stwasm_write(const char *s, unsigned n) {
	unsigned wn = 0;
	if (!stwasm_have_master)
		return -1;
	if (pty_write(stwasm_master, s, n, &wn) != 0)
		return -1;
	return (long) wn;
}
