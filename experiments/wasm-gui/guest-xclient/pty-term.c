/* M6.3 PTY test parent ("terminal emulator"). Runs as a wasm guest. Uses the host_net PTY imports to
 * spawn /pty-shell.wasm over a kernel PTY, then drives a SUSTAINED interactive session: it sends a
 * sequence of command lines to the master and waits for each expected response, proving many
 * command/response cycles round-trip both directions (terminal->shell stdin AND shell->terminal
 * stdout) over the kernel PTY. Exercises pty_spawn (open_pty_split + stdio 'pty') -> pty_write
 * (__pty_write) -> pty_read (__pty_read), plus the sidecar PTY-slave->in-session-stdin pump. */
#include <unistd.h>
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

static void puts2(const char *s) { (void) write(2, s, strlen(s)); }

static char acc[4096];
static int acc_len = 0;

/* Pump the master for up to ~bounded tries, accumulating output, until `needle` appears in everything
 * read so far. Returns 1 if seen, 0 if not within the budget. */
static int wait_for(unsigned master, const char *needle) {
    char buf[512];
    for (int tries = 0; tries < 200000; tries++) {
        if (strstr(acc, needle)) return 1;
        unsigned got = 0;
        if (pty_read(master, buf, sizeof(buf) - 1, &got) == 0 && got > 0) {
            for (unsigned k = 0; k < got && acc_len < (int) sizeof(acc) - 1; k++) acc[acc_len++] = buf[k];
            acc[acc_len] = 0;
        }
    }
    return strstr(acc, needle) ? 1 : 0;
}

static int send_line(unsigned master, const char *line) {
    char b[256];
    int n = 0;
    for (int k = 0; line[k] && n < (int) sizeof(b) - 1; k++) b[n++] = line[k];
    b[n++] = '\n';
    unsigned wn = 0;
    return pty_write(master, b, (unsigned) n, &wn) == 0 && wn == (unsigned) n;
}

int main(void) {
    const char *cmd = "/pty-shell.wasm";
    unsigned master = 0;
    if (pty_spawn(cmd, (unsigned) strlen(cmd), "[]", 2, &master) != 0) {
        puts2("PTY_SPAWN_FAIL\n");
        return 1;
    }
    puts2("PTY_SPAWN_OK\n");

    /* The shell prints "wsh ready" + a prompt on startup; confirm it ran. */
    if (!wait_for(master, "wsh ready")) { puts2("PTY_NO_CHILD_OUTPUT\n"); return 3; }
    puts2("PTY_CHILD_RAN\n");

    /* Cycle 1: terminal -> shell stdin -> shell stdout. */
    if (!send_line(master, "echo hello")) { puts2("PTY_WRITE_FAIL\n"); return 1; }
    puts2("PTY_WRITE_OK\n");
    if (!wait_for(master, "hello")) { puts2("PTY_NO_CHILD_REPLY\n"); return 2; }
    puts2("PTY_CHILD_REPLY_OK\n");

    /* Cycle 2: a different command proves it's a sustained loop, not a one-shot. */
    if (!send_line(master, "ping")) { puts2("PTY_WRITE_FAIL\n"); return 1; }
    if (!wait_for(master, "pong")) { puts2("PTY_NO_PONG\n"); return 2; }
    puts2("PTY_CHILD_PING_OK\n");

    /* Cycle 3: clean interactive shutdown. */
    if (!send_line(master, "exit")) { puts2("PTY_WRITE_FAIL\n"); return 1; }
    if (!wait_for(master, "bye")) { puts2("PTY_NO_EXIT\n"); return 2; }
    puts2("PTY_CHILD_EXIT_OK\n");

    puts2("PTY_SESSION_OK\n");
    return 0;
}
