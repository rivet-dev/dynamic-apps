/* M3 guest: a REAL third-party GUI toolkit (Nuklear) software-rendering real widgets into an RGBA
 * framebuffer, cross-compiled to wasm32-wasip1 and run INSIDE secure-exec via the M1 host.
 *
 * Nuklear is a standard, widely-used immediate-mode GUI library. Its `nuklear_rawfb` backend
 * rasterizes the UI (windows, buttons, checkboxes, sliders, text via the built-in font atlas) into
 * a plain pixel buffer — no X11, no GL, no dlopen, no threads, no font files. We emit the same
 * frame protocol v0 the M0/M1 host already speaks: b"SXFB" | u32 LE w | u32 LE h | RGBA.
 *
 * Determinism (capture mode): one frame, fixed pointer, no clock/RNG — so the output is stable.
 */
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
/* rawfb's font-query callback fills struct nk_user_font_glyph, whose full definition is gated
 * behind this define (even though rawfb rasterizes from commands, not a vertex buffer). */
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_IMPLEMENTATION
#define NK_RAWFB_IMPLEMENTATION
#include "nuklear.h"
#include "nuklear_rawfb.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define W 640u
#define H 480u

static void write_frame(const char *path, const unsigned char *fb) {
    FILE *f = path ? fopen(path, "wb") : stdout;
    if (!f) { perror("open out"); exit(1); }
    unsigned char hdr[12];
    memcpy(hdr, "SXFB", 4);
    unsigned int w = W, h = H;
    memcpy(hdr + 4, &w, 4); /* wasm is little-endian -> LE u32, matches the protocol */
    memcpy(hdr + 8, &h, 4);
    fwrite(hdr, 1, 12, f);
    fwrite(fb, 1, W * H * 4, f);
    if (f != stdout) fclose(f);
}

/* Build the UI for one frame. Real Nuklear widgets, not hand-rolled drawing. */
static int g_check = 1;
static float g_slider = 0.6f;
static nk_size g_prog = 64;
static int g_option = 1;

static void build_ui(struct nk_context *ctx) {
    if (nk_begin(ctx, "secure-exec - Nuklear (wasm, in the sidecar)",
                 nk_rect(48, 54, 380, 300),
                 NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_MOVABLE)) {
        nk_layout_row_dynamic(ctx, 22, 1);
        nk_label(ctx, "Real toolkit widgets, software-rendered:", NK_TEXT_LEFT);

        nk_layout_row_dynamic(ctx, 30, 2);
        nk_button_label(ctx, "Build");
        nk_button_label(ctx, "Run");

        nk_layout_row_dynamic(ctx, 26, 1);
        nk_checkbox_label(ctx, "Enable sandbox", &g_check);

        nk_layout_row_dynamic(ctx, 22, 2);
        if (nk_option_label(ctx, "wasm32", g_option == 1)) g_option = 1;
        if (nk_option_label(ctx, "native", g_option == 0)) g_option = 0;

        nk_layout_row_dynamic(ctx, 26, 1);
        nk_slider_float(ctx, 0.0f, &g_slider, 1.0f, 0.01f);

        nk_layout_row_dynamic(ctx, 24, 1);
        nk_progress(ctx, &g_prog, 100, NK_FIXED);

        nk_layout_row_dynamic(ctx, 22, 1);
        nk_label(ctx, "rendered by Nuklear, inside secure-exec", NK_TEXT_LEFT);
    }
    nk_end(ctx);

    /* A second small window, like a desktop with multiple panels. */
    if (nk_begin(ctx, "Info", nk_rect(446, 150, 150, 150),
                 NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_MOVABLE)) {
        nk_layout_row_dynamic(ctx, 20, 1);
        nk_label(ctx, "M3: real toolkit", NK_TEXT_LEFT);
        nk_label(ctx, "no X, no GL,", NK_TEXT_LEFT);
        nk_label(ctx, "no dlopen.", NK_TEXT_LEFT);
    }
    nk_end(ctx);
}

int main(int argc, char **argv) {
    const char *out = NULL;
    int loop = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--out") && i + 1 < argc) out = argv[++i];
        else if (!strcmp(argv[i], "--loop")) loop = 1;
    }

    unsigned char *fb = calloc(W * H, 4);
    unsigned char *tex = malloc(1024 * 1024); /* font-atlas scratch (alpha8) */
    if (!fb || !tex) { fprintf(stderr, "oom\n"); return 1; }

    /* Pixel layout: little-endian uint per pixel => bytes [R,G,B,A]. */
    struct rawfb_pl pl;
    pl.bytesPerPixel = 4;
    pl.rshift = 0; pl.gshift = 8; pl.bshift = 16; pl.ashift = 24;
    pl.rloss = 0; pl.gloss = 0; pl.bloss = 0; pl.aloss = 0;

    struct rawfb_context *rawfb = nk_rawfb_init(fb, tex, W, H, W * 4, pl);
    if (!rawfb) { fprintf(stderr, "rawfb init failed\n"); return 1; }

    struct nk_color clear = nk_rgb(28, 64, 96); /* desktop wallpaper */

    if (!loop) {
        /* Capture mode: one deterministic frame with a fixed pointer. */
        nk_input_begin(&rawfb->ctx);
        nk_input_motion(&rawfb->ctx, 300, 235);
        nk_input_end(&rawfb->ctx);
        build_ui(&rawfb->ctx);
        nk_rawfb_render(rawfb, clear, 1);
        write_frame(out, fb);
        nk_rawfb_shutdown(rawfb);
        free(fb); free(tex);
        return 0;
    }

    /* Window mode: read input tokens from stdin, render frames to stdout. */
    int px = W / 2, py = H / 2, down = 0;
    char line[128];
    /* initial frame */
    nk_input_begin(&rawfb->ctx);
    nk_input_end(&rawfb->ctx);
    build_ui(&rawfb->ctx);
    nk_rawfb_render(rawfb, clear, 1);
    write_frame(NULL, fb);
    fflush(stdout);
    while (fgets(line, sizeof(line), stdin)) {
        if (line[0] == 'q') break;
        if (line[0] == 'p') sscanf(line + 1, "%d %d", &px, &py);
        if (line[0] == 'd') down = 1;
        if (line[0] == 'u') down = 0;
        nk_input_begin(&rawfb->ctx);
        nk_input_motion(&rawfb->ctx, px, py);
        nk_input_button(&rawfb->ctx, NK_BUTTON_LEFT, px, py, down);
        nk_input_end(&rawfb->ctx);
        build_ui(&rawfb->ctx);
        nk_rawfb_render(rawfb, clear, 1);
        write_frame(NULL, fb);
        fflush(stdout);
    }
    nk_rawfb_shutdown(rawfb);
    free(fb); free(tex);
    return 0;
}
