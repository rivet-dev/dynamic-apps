
#ifndef __foo_marshal_MARSHAL_H__
#define __foo_marshal_MARSHAL_H__

#include	<glib-object.h>

#ifdef __cplusplus
extern "C" {
#endif

/* VOID:OBJECT,OBJECT (marshal.list:1) */
extern void foo_marshal_VOID__OBJECT_OBJECT (GClosure     *closure,
                                             GValue       *return_value,
                                             guint         n_param_values,
                                             const GValue *param_values,
                                             gpointer      invocation_hint,
                                             gpointer      marshal_data);

/* VOID:UINT,UINT,UINT,UINT (marshal.list:2) */
extern void foo_marshal_VOID__UINT_UINT_UINT_UINT (GClosure     *closure,
                                                   GValue       *return_value,
                                                   guint         n_param_values,
                                                   const GValue *param_values,
                                                   gpointer      invocation_hint,
                                                   gpointer      marshal_data);

/* VOID:UINT,UINT (marshal.list:3) */
extern void foo_marshal_VOID__UINT_UINT (GClosure     *closure,
                                         GValue       *return_value,
                                         guint         n_param_values,
                                         const GValue *param_values,
                                         gpointer      invocation_hint,
                                         gpointer      marshal_data);

/* VOID:BOXED (marshal.list:4) */
#define foo_marshal_VOID__BOXED	g_cclosure_marshal_VOID__BOXED

/* VOID:BOXED,BOXED (marshal.list:5) */
extern void foo_marshal_VOID__BOXED_BOXED (GClosure     *closure,
                                           GValue       *return_value,
                                           guint         n_param_values,
                                           const GValue *param_values,
                                           gpointer      invocation_hint,
                                           gpointer      marshal_data);

/* VOID:POINTER,BOXED,POINTER (marshal.list:6) */
extern void foo_marshal_VOID__POINTER_BOXED_POINTER (GClosure     *closure,
                                                     GValue       *return_value,
                                                     guint         n_param_values,
                                                     const GValue *param_values,
                                                     gpointer      invocation_hint,
                                                     gpointer      marshal_data);

/* VOID:POINTER,POINTER (marshal.list:7) */
extern void foo_marshal_VOID__POINTER_POINTER (GClosure     *closure,
                                               GValue       *return_value,
                                               guint         n_param_values,
                                               const GValue *param_values,
                                               gpointer      invocation_hint,
                                               gpointer      marshal_data);

#ifdef __cplusplus
}
#endif

#endif /* __foo_marshal_MARSHAL_H__ */

