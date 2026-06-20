/* Minimal FreeType probe: open a TTF directly with FT_New_Face and report the result, isolating
 * FreeType-in-wasm from fontconfig. If this loads the face, the Xft 0-fonts problem is in fontconfig;
 * if it fails, the freetype build/runtime is the culprit. Prints to stderr; no X needed. */
#include <ft2build.h>
#include FT_FREETYPE_H
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void mark(const char *s) { write(2, s, strlen(s)); }

int main(void) {
    FT_Library lib;
    char buf[128];
    FT_Error e = FT_Init_FreeType(&lib);
    snprintf(buf, sizeof(buf), "FT:init=%d\n", e); mark(buf);
    if (e) return 1;

    #include <stdlib.h>
    const char *path = "/usr/share/fonts/truetype/DejaVuSans.ttf";
    /* Load the whole TTF into memory and try FT_New_Memory_Face (bypasses freetype's file stream). */
    {
        FILE *fm = fopen(path, "rb");
        if (fm) {
            fseek(fm, 0, SEEK_END); long ms = ftell(fm); fseek(fm, 0, SEEK_SET);
            unsigned char *mb = malloc(ms);
            size_t got = fread(mb, 1, ms, fm); fclose(fm);
            FT_Face mf;
            FT_Error me = FT_New_Memory_Face(lib, mb, (FT_Long)got, 0, &mf);
            snprintf(buf, sizeof(buf), "FT:mem_face=%d (read %zu/%ld)\n", me, got, ms); mark(buf);
            if (!me) { snprintf(buf, sizeof(buf), "FT:mem_family=%s glyphs=%ld\n", mf->family_name?mf->family_name:"?", (long)mf->num_glyphs); mark(buf); }
        }
    }
    /* Compare plain fopen on the TTF vs a path fontconfig opened fine (/etc/fonts/fonts.conf). */
    FILE *f1 = fopen(path, "rb");
    snprintf(buf, sizeof(buf), "FT:fopen_ttf=%s\n", f1 ? "ok" : "FAIL"); mark(buf);
    if (f1) {
        char t[4]; size_t n = fread(t, 1, 4, f1);
        fseek(f1, 0, SEEK_END); long sz = ftell(f1);
        snprintf(buf, sizeof(buf), "FT:ttf_read4=%zu seek_end_size=%ld\n", n, sz); mark(buf);
        fclose(f1);
    }
    FILE *f2 = fopen("/etc/fonts/fonts.conf", "rb");
    snprintf(buf, sizeof(buf), "FT:fopen_conf=%s\n", f2 ? "ok" : "FAIL"); mark(buf);
    if (f2) fclose(f2);
    FT_Face face;
    e = FT_New_Face(lib, path, 0, &face);
    snprintf(buf, sizeof(buf), "FT:new_face=%d\n", e); mark(buf);
    if (e) return 2;

    snprintf(buf, sizeof(buf), "FT:num_faces=%ld num_glyphs=%ld family=%s\n",
             (long)face->num_faces, (long)face->num_glyphs,
             face->family_name ? face->family_name : "(null)");
    mark(buf);

    e = FT_Set_Pixel_Sizes(face, 0, 24);
    snprintf(buf, sizeof(buf), "FT:set_size=%d\n", e); mark(buf);
    e = FT_Load_Char(face, 'A', FT_LOAD_RENDER);
    snprintf(buf, sizeof(buf), "FT:load_A=%d bitmap=%dx%d\n", e,
             face->glyph->bitmap.width, face->glyph->bitmap.rows); mark(buf);
    mark("FT:ok\n");
    return 0;
}
