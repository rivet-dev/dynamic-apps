/* Time each stage of rendering one line of text (in-memory cairo surface, no X) to find the ~13s
 * bottleneck: cairo setup vs pango layout/shaping (fontconfig+freetype+harfbuzz) vs glyph rasterization.
 * A second label measures whether the cost is one-time (font/glyph-cache setup) or per-label. */
#include <pango/pangocairo.h>
#include <stdio.h>
#include <time.h>
static double ms(void){ struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return t.tv_sec*1000.0+t.tv_nsec/1e6; }
int main(void){
  double a=ms();
  cairo_surface_t *sf=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,400,100);
  cairo_t *cr=cairo_create(sf);
  double b=ms();
  PangoLayout *l=pango_cairo_create_layout(cr);
  pango_layout_set_text(l,"Hello notification, all wasm.",-1);
  double c=ms();
  int w,h; pango_layout_get_pixel_size(l,&w,&h);   /* forces fontconfig match + FT face + harfbuzz shape */
  double d=ms();
  cairo_move_to(cr,5,5); pango_cairo_show_layout(cr,l);  /* rasterize glyphs */
  cairo_surface_flush(sf);
  double e=ms();
  fprintf(stderr,"PANGO-BENCH: setup=%.0f create+set=%.0f shape(get_size)=%.0f rasterize(show)=%.0f size=%dx%d\n",b-a,c-b,d-c,e-d,w,h);
  double f=ms();
  PangoLayout *l2=pango_cairo_create_layout(cr); pango_layout_set_text(l2,"Second different line.",-1);
  pango_layout_get_pixel_size(l2,&w,&h); cairo_move_to(cr,5,40); pango_cairo_show_layout(cr,l2);
  fprintf(stderr,"PANGO-BENCH: SECOND label total=%.0fms (warm)\n",ms()-f);
  return 0;
}
