
#ifndef __fm_marshal_MARSHAL_H__
#define __fm_marshal_MARSHAL_H__

#include	<glib-object.h>

G_BEGIN_DECLS

/* BOOL:INT,INT,UINT,UINT,POINTER (gtk/fm-gtk-marshal.list:1) */
extern void fm_marshal_BOOLEAN__INT_INT_UINT_UINT_POINTER (GClosure     *closure,
                                                           GValue       *return_value,
                                                           guint         n_param_values,
                                                           const GValue *param_values,
                                                           gpointer      invocation_hint,
                                                           gpointer      marshal_data);
#define fm_marshal_BOOL__INT_INT_UINT_UINT_POINTER	fm_marshal_BOOLEAN__INT_INT_UINT_UINT_POINTER

/* VOID:BOXED,BOXED,POINTER (gtk/fm-gtk-marshal.list:2) */
extern void fm_marshal_VOID__BOXED_BOXED_POINTER (GClosure     *closure,
                                                  GValue       *return_value,
                                                  guint         n_param_values,
                                                  const GValue *param_values,
                                                  gpointer      invocation_hint,
                                                  gpointer      marshal_data);

/* VOID:OBJECT,OBJECT,POINTER (gtk/fm-gtk-marshal.list:3) */
extern void fm_marshal_VOID__OBJECT_OBJECT_POINTER (GClosure     *closure,
                                                    GValue       *return_value,
                                                    guint         n_param_values,
                                                    const GValue *param_values,
                                                    gpointer      invocation_hint,
                                                    gpointer      marshal_data);

G_END_DECLS

#endif /* __fm_marshal_MARSHAL_H__ */

