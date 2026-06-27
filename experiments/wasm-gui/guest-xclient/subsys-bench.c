/* Decompose gtk_init's ~11s into subsystems (PERF-OPTIMIZATION.md L-G/L-H/L-I). NO gtk_init, NO X:
 * time the heavy non-GObject subsystems gtk_init drives internally — fontconfig (FcInit: scan/parse
 * fonts + build cache), pango (font map + first text shaping via HarfBuzz), cairo (image-surface
 * render). Each prints ONE ms number to stderr. Whichever dominates localizes the startup cost.
 * Runs via --exec (cairo image surface needs no display). */
#include <fontconfig/fontconfig.h>
#include <pango/pangocairo.h>
#include <cairo.h>
#include <stdio.h>
#include <time.h>

static double ms(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}

int main(void) {
  double t0 = ms();
  FcInit();
  double t1 = ms();
  fprintf(stderr, "SUBSYS: FcInit = %.0fms\n", t1 - t0);
  fflush(stderr);

  double t2 = ms();
  PangoFontMap *fm = pango_cairo_font_map_get_default();
  PangoContext *pc = pango_font_map_create_context(fm);
  double t3 = ms();
  fprintf(stderr, "SUBSYS: pango fontmap+context = %.0fms\n", t3 - t2);
  fflush(stderr);
  (void)pc;

  double t4 = ms();
  cairo_surface_t *surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 800, 600);
  cairo_t *cr = cairo_create(surf);
  PangoLayout *layout = pango_cairo_create_layout(cr);
  pango_layout_set_text(layout, "The quick brown fox jumps over the lazy dog 0123456789", -1);
  PangoFontDescription *fd = pango_font_description_from_string("Sans 12");
  pango_layout_set_font_description(layout, fd);
  pango_font_description_free(fd);
  double t5 = ms();
  int w = 0, h = 0;
  pango_layout_get_pixel_size(layout, &w, &h); /* forces font load + shaping (HarfBuzz) */
  double t6 = ms();
  fprintf(stderr, "SUBSYS: pango first-layout+shape = %.0fms (%dx%d)\n", t6 - t5, w, h);
  fflush(stderr);
  pango_cairo_show_layout(cr, layout);
  cairo_surface_flush(surf);
  double t7 = ms();
  fprintf(stderr, "SUBSYS: cairo render = %.0fms\n", t7 - t6);
  fprintf(stderr, "SUBSYS: (setup cairo objs = %.0fms) TOTAL fc+pango+layout+cairo = %.0fms\n",
          t5 - t4, t7 - t0);
  fflush(stderr);
  return 0;
}
