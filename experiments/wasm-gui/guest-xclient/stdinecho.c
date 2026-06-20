/* Minimal host->guest stdin probe: read lines from stdin and echo each to stderr with a marker.
 * Used to verify the host's write_stdin (e.g. via --inject) reaches a top-level wasm guest's reads,
 * which is the channel the live desktop needs to forward input to the XTEST agent. No X needed. */
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void mark(const char *s) { write(2, s, strlen(s)); }

int main(void) {
    mark("SE:ready\n");
    char line[256];
    while (fgets(line, sizeof(line), stdin)) {
        mark("SE:got:");
        mark(line);
        if (line[strlen(line) - 1] != '\n') mark("\n");
    }
    mark("SE:eof\n");
    return 0;
}
