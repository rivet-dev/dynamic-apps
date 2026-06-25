/* Isolate the GTK CSS PARSE from the style cascade: parse a large synthetic stylesheet via
 * gtk_css_provider_load_from_data and time it. If ~ms/rule -> the parser is fine and the ~12s
 * first-widget cost is the cascade (indirect-call-heavy = fpcast-emu). If ~ms*N -> the parser itself
 * is the wasm bottleneck (a string/tokenizer perf issue, a potentially shallower fix). */
#include <gtk/gtk.h>
#include <stdio.h>
#include <time.h>
static double ms(void){ struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return t.tv_sec*1000.0+t.tv_nsec/1e6; }
int main(int argc,char**argv){
  gtk_init(&argc,&argv);
  fprintf(stderr,"CSS-BENCH: gtk_init done\n");
  GString *s=g_string_new("");
  int N=4000;
  for(int i=0;i<N;i++) g_string_append_printf(s,".c%d label { color: rgb(%d,%d,%d); padding: %dpx; margin: %dpx; border: 1px solid #abc; }\n",i,i%255,(i*3)%255,(i*7)%255,i%12,i%8);
  GtkCssProvider *p=gtk_css_provider_new();
  double t0=ms();
  gtk_css_provider_load_from_data(p,s->str,s->len,NULL);
  double t1=ms();
  fprintf(stderr,"CSS-BENCH: parse %d rules = %.0fms (%.3fms/rule, %d bytes)\n",N,t1-t0,(t1-t0)/N,(int)s->len);
  /* add the extra rules to the screen, then time ONE label cascade vs the popup-repro baseline (~13s, Adwaita only) */
  gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(p), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  GtkWidget *win=gtk_window_new(GTK_WINDOW_TOPLEVEL); GtkWidget *lbl=gtk_label_new("hello"); gtk_container_add(GTK_CONTAINER(win),lbl);
  double t2=ms(); GtkRequisition req; gtk_widget_get_preferred_size(win,NULL,&req); double t3=ms();
  fprintf(stderr,"CSS-BENCH: first-label cascade (Adwaita + %d extra rules) = %.0fms\n",N,t3-t2);
  return 0;
}
