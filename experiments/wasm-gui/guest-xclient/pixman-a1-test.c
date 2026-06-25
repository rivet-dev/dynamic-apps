/* Constraint #4 bottom-level localization: does pixman fill an A1 (1bpp) image correctly in the wasm
 * build? cairo renders xfwm4's decoration SHAPE mask into a pixman A1 image client-side, then uploads it.
 * Every X-server A1 primitive works (xrender-a1-test), cairo->color-pixmap works, yet cairo->A1-mask is
 * empty -> the suspect is pixman's A1 fill. Fill the LEFT HALF of an A1 image opaque, read back the bits,
 * and count set bits. Expect ~ (W/2)*H set. If 0 -> pixman A1 fill is broken = the platform fix point. */
#include <pixman.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

static void mark(const char *m) { write(2, m, strlen(m)); }

int main(void) {
    mark("PX:start\n");
    int W = 64, H = 32;
    pixman_image_t *dst = pixman_image_create_bits(PIXMAN_a1, W, H, NULL, 0);
    if (!dst) { mark("PX:create_failed\n"); return 1; }
    mark("PX:created\n");

    pixman_color_t opaque = {0, 0, 0, 0xffff};
    pixman_box32_t box = { 0, 0, W/2, H };           /* left half */
    pixman_bool_t ok = pixman_image_fill_boxes(PIXMAN_OP_SRC, dst, &opaque, 1, &box);
    char b[96]; snprintf(b, sizeof b, "PX:fill_boxes ret=%d\n", ok); mark(b);

    /* Read back and count set bits across the whole image. */
    uint32_t *data = pixman_image_get_data(dst);
    int stride = pixman_image_get_stride(dst);       /* bytes per row */
    int set = 0, total = W * H;
    for (int y = 0; y < H; y++) {
        unsigned char *row = (unsigned char *)data + (size_t)y * stride;
        for (int x = 0; x < W; x++) {
            /* pixman A1: bit x within the row; use the same packing pixman uses (bit (x&31) of the
             * 32-bit word, native order). Count via byte+bit for robustness across endianness. */
            int word = x >> 5;
            uint32_t w = ((uint32_t *)row)[word];
            if (w & (1u << (x & 31))) set++;
        }
    }
    snprintf(b, sizeof b, "PX:set_bits=%d / %d (expect ~%d for left half)\n", set, total, (W/2)*H); mark(b);
    mark(set > 0 ? "PX:RESULT=pixman_A1_WORKS\n" : "PX:RESULT=pixman_A1_BROKEN\n");
    return 0;
}
