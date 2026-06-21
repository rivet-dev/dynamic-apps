/* M6.3 interactive "shell" child. Runs as a wasm guest whose stdin/stdout are a kernel PTY slave
 * (wired by __pty_spawn's stdio 'pty' mode). A real line-oriented interpreter: it prints a prompt,
 * reads a command line from fd 0 (the PTY slave), and responds, looping until it reads "exit". This
 * proves SUSTAINED interactive terminal I/O (many command/response cycles), not a one-shot echo.
 *
 * Commands: "echo <text>" -> "<text>"; "ping" -> "pong"; "exit" -> quits. Anything else -> "?: <line>".
 * A real shell (dash/bash) needs fork/exec/job-control, which wasi lacks; this is a faithful
 * interactive interpreter over the same PTY primitive a terminal emulator would drive. */
#include <unistd.h>
#include <string.h>
#include <stdio.h>

static void puts1(const char *s) { (void) write(1, s, strlen(s)); }

/* Read one newline-terminated line from fd 0 into buf (NUL-terminated, newline stripped). Returns the
 * length, or -1 on EOF. Retries on transient empty reads (the PTY slave can return 0 before the
 * terminal's master write has been pumped through the line discipline). */
static int read_line(char *buf, int cap) {
    int len = 0;
    for (int spins = 0; spins < 2000000; spins++) {
        char c;
        ssize_t n = read(0, &c, 1);
        if (n <= 0) continue;            /* transient empty read; keep polling the slave */
        if (c == '\r') continue;
        if (c == '\n') { buf[len] = 0; return len; }
        if (len < cap - 1) buf[len++] = c;
    }
    buf[len] = 0;
    return len;
}

static int starts_with(const char *s, const char *p) {
    return strncmp(s, p, strlen(p)) == 0;
}

int main(void) {
    char line[256];
    puts1("wsh ready\n");
    for (;;) {
        puts1("$ ");                      /* prompt */
        int n = read_line(line, sizeof(line));
        if (n < 0) break;
        /* secure-exec: append every received command line to a VM file so a test harness can
         * INDEPENDENTLY verify that host-typed keystrokes traversed the whole interactive chain
         * (host XTEST -> X server -> terminal emulator -> kernel PTY -> this shell), without relying
         * on the terminal's own reporting or on the rendered framebuffer. */
        if (n > 0) {
            FILE *fp = fopen("/data/shell_in.txt", "a");
            if (fp) { fprintf(fp, "%s\n", line); fclose(fp); }
        }
        if (strcmp(line, "exit") == 0) { puts1("bye\n"); break; }
        if (strcmp(line, "ping") == 0) { puts1("pong\n"); continue; }
        if (starts_with(line, "echo ")) {
            puts1(line + 5);
            puts1("\n");
            continue;
        }
        if (n == 0) continue;             /* empty line: just re-prompt */
        puts1("?: ");
        puts1(line);
        puts1("\n");
    }
    return 0;
}
