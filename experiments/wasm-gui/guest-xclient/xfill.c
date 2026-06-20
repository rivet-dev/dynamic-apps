/*
 * Minimal raw X11 client (no libX11): connects to the secure-exec wasm Xvfb over the
 * AF_UNIX socket /tmp/.X11-unix/X0, performs the X11 connection-setup handshake, then
 * fills the root window with a solid color using core protocol requests (CreateGC +
 * PolyFillRectangle). This proves an X client can reach our wasm X server over the
 * shared kernel socket table and drive the framebuffer. Stages are reported on stderr
 * with "CMARK:" markers so the host can verify progress.
 */
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>

static void mark(const char *m) { write(2, m, strlen(m)); }

/* The patched wasi-libc routes send()/recv() (not write()/read()) to host_net, so all
 * socket I/O on the host_net fd must use the socket calls. */
static int read_full(int fd, void *buf, size_t n) {
    uint8_t *p = (uint8_t *)buf;
    size_t got = 0;
    while (got < n) {
        ssize_t r = recv(fd, p + got, n - got, 0);
        if (r > 0) { got += (size_t)r; continue; }
        if (r < 0 && (errno == EINTR || errno == EAGAIN)) continue;
        return -1;
    }
    return 0;
}

static int write_full(int fd, const void *buf, size_t n) {
    const uint8_t *p = (const uint8_t *)buf;
    size_t put = 0;
    while (put < n) {
        ssize_t w = send(fd, p + put, n - put, 0);
        if (w > 0) { put += (size_t)w; continue; }
        if (w < 0 && (errno == EINTR || errno == EAGAIN)) continue;
        return -1;
    }
    return 0;
}

static int arg_int(int argc, char **argv, int idx, int dflt) {
    if (idx >= argc) return dflt;
    int v = 0, sign = 1; const char *s = argv[idx];
    if (*s == '-') { sign = -1; s++; }
    int any = 0;
    for (; *s >= '0' && *s <= '9'; s++) { v = v*10 + (*s - '0'); any = 1; }
    return any ? sign*v : dflt;
}

int main(int argc, char **argv) {
    mark("CMARK:start\n");

    /* Optional argv: x y w h color(decimal). Defaults to a full-screen orange fill. */
    int want_x = arg_int(argc, argv, 1, 0);
    int want_y = arg_int(argc, argv, 2, 0);
    int want_w = arg_int(argc, argv, 3, -1);   /* -1 => full width */
    int want_h = arg_int(argc, argv, 4, -1);   /* -1 => full height */
    unsigned want_color = (unsigned) arg_int(argc, argv, 5, (int) 0x00FF8800u);

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) { mark("CMARK:socket_fail\n"); return 1; }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, "/tmp/.X11-unix/X0");
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        mark("CMARK:connect_fail\n");
        return 1;
    }
    mark("CMARK:connected\n");

    /* Connection setup request: little-endian, protocol 11.0, no auth. */
    uint8_t setup[12] = {0};
    setup[0] = 0x6c;          /* 'l' little-endian */
    setup[2] = 11; setup[3] = 0;  /* major 11 */
    setup[4] = 0;  setup[5] = 0;  /* minor 0 */
    /* auth name/data lengths = 0 */
    if (write_full(fd, setup, sizeof(setup)) != 0) { mark("CMARK:setup_write_fail\n"); return 1; }
    mark("CMARK:setup_sent\n");

    /* Setup reply header (8 bytes). */
    uint8_t hdr[8];
    if (read_full(fd, hdr, 8) != 0) { mark("CMARK:reply_read_fail\n"); return 1; }
    if (hdr[0] != 1) { mark("CMARK:setup_refused\n"); return 1; }
    mark("CMARK:setup_accepted\n");

    uint16_t add_len_words = (uint16_t)(hdr[6] | (hdr[7] << 8));
    size_t add_len = (size_t)add_len_words * 4;
    uint8_t *body = (uint8_t *)__builtin_alloca(add_len);
    if (read_full(fd, body, add_len) != 0) { mark("CMARK:body_read_fail\n"); return 1; }

    /* Parse the fixed part of the setup reply additional data. */
    uint32_t id_base = body[4] | (body[5]<<8) | (body[6]<<16) | ((uint32_t)body[7]<<24);
    uint16_t vendor_len = body[16] | (body[17]<<8);
    uint8_t num_formats = body[21];

    /* Skip: 32 bytes fixed + vendor (padded to 4) + 8*num_formats pixmap formats. */
    size_t off = 32 + ((vendor_len + 3) & ~3u) + 8 * (size_t)num_formats;
    /* First SCREEN: root window id at offset 0, root visual at offset 32. */
    uint32_t root = body[off+0] | (body[off+1]<<8) | (body[off+2]<<16) | ((uint32_t)body[off+3]<<24);
    uint16_t width  = body[off+20] | (body[off+21]<<8);
    uint16_t height = body[off+22] | (body[off+23]<<8);
    mark("CMARK:parsed_screen\n");

    uint32_t gc = id_base | 1;

    /* CreateGC (opcode 55): set GCForeground to a bright color. */
    uint8_t cg[20];
    cg[0]=55; cg[1]=0; cg[2]=5; cg[3]=0;          /* opcode, pad, length=5 words */
    memcpy(cg+4, &gc, 4);
    memcpy(cg+8, &root, 4);
    uint32_t mask = 0x00000004;                    /* GCForeground */
    memcpy(cg+12, &mask, 4);
    uint32_t fg = want_color;
    memcpy(cg+16, &fg, 4);
    if (write_full(fd, cg, sizeof(cg)) != 0) { mark("CMARK:creategc_fail\n"); return 1; }

    /* PolyFillRectangle (opcode 70): one rect (argv-positioned, or whole root window). */
    uint8_t pf[20];
    pf[0]=70; pf[1]=0; pf[2]=5; pf[3]=0;            /* opcode, pad, length=5 words (3 + 2*1) */
    memcpy(pf+4, &root, 4);
    memcpy(pf+8, &gc, 4);
    uint16_t x = (uint16_t) want_x, y = (uint16_t) want_y;
    uint16_t rw = (want_w < 0) ? width  : (uint16_t) want_w;
    uint16_t rh = (want_h < 0) ? height : (uint16_t) want_h;
    memcpy(pf+12, &x, 2); memcpy(pf+14, &y, 2);
    memcpy(pf+16, &rw, 2); memcpy(pf+18, &rh, 2);
    if (write_full(fd, pf, sizeof(pf)) != 0) { mark("CMARK:fill_fail\n"); return 1; }
    mark("CMARK:filled\n");

    /* GetInputFocus (opcode 43) as a round-trip barrier: forces the server to process
     * our queued requests and reply, guaranteeing the fill reached the framebuffer. */
    uint8_t gif[4] = {43, 0, 1, 0};
    if (write_full(fd, gif, sizeof(gif)) != 0) { mark("CMARK:sync_write_fail\n"); return 1; }
    uint8_t rep[32];
    if (read_full(fd, rep, 32) != 0) { mark("CMARK:sync_read_fail\n"); return 1; }
    mark("CMARK:synced\n");
    mark("CMARK:done\n");
    return 0;
}
