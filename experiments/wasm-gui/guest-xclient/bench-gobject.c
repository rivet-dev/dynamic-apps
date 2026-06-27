/* B1 (PERF-OPTIMIZATION.md): pure-GObject benchmark — NO GTK/X/D-Bus. Isolates the GObject dispatch
 * cost (the L-B "compute" suspect: type/instance machinery + signal marshaling via the generic
 * marshaler -> the runtime's ffi_call, all indirect-call-heavy = fpcast-emu). Emits ONE easy-to-measure
 * number per op kind (us/op). Compare to native (~0.05-0.5us/op) for the slowdown factor.
 *
 * Times three GObject hot paths, N each:
 *   1) g_object_new + unref     (instance construction: constructor/init vfuncs, property defaults)
 *   2) g_signal_emit            (generic-marshaled signal -> g_cclosure_marshal_generic -> ffi_call)
 *   3) g_object_set + g_object_get (property set/get vfuncs)
 *
 * In-guest timing via CLOCK_MONOTONIC (advances; ms resolution is fine over N ops taking >>1ms).
 */
#include <glib-object.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BENCH_TYPE_OBJ (bench_obj_get_type())
G_DECLARE_FINAL_TYPE(BenchObj, bench_obj, BENCH, OBJ, GObject)
struct _BenchObj {
  GObject parent;
  int val;
};
G_DEFINE_TYPE(BenchObj, bench_obj, G_TYPE_OBJECT)

enum { PROP_0, PROP_VAL, N_PROPS };
static GParamSpec *props[N_PROPS];
static guint sig_ping;
static volatile long counter = 0;

static void on_ping(BenchObj *o, int x, gpointer d) {
  (void)o;
  (void)d;
  counter += x;
}
static void bench_get(GObject *o, guint id, GValue *v, GParamSpec *p) {
  if (id == PROP_VAL)
    g_value_set_int(v, ((BenchObj *)o)->val);
  else
    G_OBJECT_WARN_INVALID_PROPERTY_ID(o, id, p);
}
static void bench_set(GObject *o, guint id, const GValue *v, GParamSpec *p) {
  if (id == PROP_VAL)
    ((BenchObj *)o)->val = g_value_get_int(v);
  else
    G_OBJECT_WARN_INVALID_PROPERTY_ID(o, id, p);
}
static void bench_obj_init(BenchObj *o) { o->val = 0; }
static void bench_obj_class_init(BenchObjClass *k) {
  GObjectClass *gc = G_OBJECT_CLASS(k);
  gc->get_property = bench_get;
  gc->set_property = bench_set;
  props[PROP_VAL] =
      g_param_spec_int("val", "val", "val", 0, 1000000, 0, G_PARAM_READWRITE);
  g_object_class_install_properties(gc, N_PROPS, props);
  /* NULL c_marshaller => generic (ffi-based) marshaler. */
  sig_ping = g_signal_new("ping", BENCH_TYPE_OBJ, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                          NULL, G_TYPE_NONE, 1, G_TYPE_INT);
}

static double now_ms(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}

int main(int argc, char **argv) {
  long n = (argc > 1) ? atol(argv[1]) : 100000;

  double t0 = now_ms();
  for (long i = 0; i < n; i++) {
    BenchObj *o = g_object_new(BENCH_TYPE_OBJ, NULL);
    g_object_unref(o);
  }
  double t1 = now_ms();

  BenchObj *o = g_object_new(BENCH_TYPE_OBJ, NULL);
  g_signal_connect(o, "ping", G_CALLBACK(on_ping), NULL);
  double t2 = now_ms();
  for (long i = 0; i < n; i++)
    g_signal_emit(o, sig_ping, 0, 1);
  double t3 = now_ms();

  double t4 = now_ms();
  for (long i = 0; i < n; i++) {
    g_object_set(o, "val", (int)(i & 0xffff), NULL);
    int v = 0;
    g_object_get(o, "val", &v, NULL);
  }
  double t5 = now_ms();
  g_object_unref(o);

  printf("BENCH-GOBJECT n=%ld | new+unref=%.0fms (%.3f us/op) | emit=%.0fms (%.3f us/op) | "
         "set+get=%.0fms (%.3f us/op) | counter=%ld\n",
         n, t1 - t0, (t1 - t0) * 1000.0 / n, t3 - t2, (t3 - t2) * 1000.0 / n, t5 - t4,
         (t5 - t4) * 1000.0 / n, counter);
  fflush(stdout);
  return 0;
}
