
#ifndef __fm_marshal_MARSHAL_H__
#define __fm_marshal_MARSHAL_H__

#include	<glib-object.h>

G_BEGIN_DECLS

/* INT:POINTER,POINTER (base/fm-marshal.list:1) */
extern void fm_marshal_INT__POINTER_POINTER (GClosure     *closure,
                                             GValue       *return_value,
                                             guint         n_param_values,
                                             const GValue *param_values,
                                             gpointer      invocation_hint,
                                             gpointer      marshal_data);

/* INT:POINTER,INT (base/fm-marshal.list:2) */
extern void fm_marshal_INT__POINTER_INT (GClosure     *closure,
                                         GValue       *return_value,
                                         guint         n_param_values,
                                         const GValue *param_values,
                                         gpointer      invocation_hint,
                                         gpointer      marshal_data);

/* INT:POINTER,POINTER,POINTER (base/fm-marshal.list:3) */
extern void fm_marshal_INT__POINTER_POINTER_POINTER (GClosure     *closure,
                                                     GValue       *return_value,
                                                     guint         n_param_values,
                                                     const GValue *param_values,
                                                     gpointer      invocation_hint,
                                                     gpointer      marshal_data);

/* UINT:BOXED,UINT (base/fm-marshal.list:4) */
extern void fm_marshal_UINT__BOXED_UINT (GClosure     *closure,
                                         GValue       *return_value,
                                         guint         n_param_values,
                                         const GValue *param_values,
                                         gpointer      invocation_hint,
                                         gpointer      marshal_data);

G_END_DECLS

#endif /* __fm_marshal_MARSHAL_H__ */

