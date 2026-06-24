#include "openbox/actions.h"
#include <glib.h>

typedef struct {
    gchar   *str;
} Options;

static gpointer setup_func(xmlNodePtr node);
static void     free_func(gpointer options);
static gboolean run_func(ObActionsData *data, gpointer options);

void action_debug_startup(void)
{
    actions_register("Debug", setup_func, free_func, run_func);
}

static gpointer setup_func(xmlNodePtr node)
{
    xmlNodePtr n;
    Options *o;

    o = g_slice_new0(Options);

    if ((n = obt_xml_find_node(node, "string")))
        o->str = obt_xml_node_string(n);
    return o;
}

static void free_func(gpointer options)
{
    Options *o = options;
    g_free(o->str);
    g_slice_free(Options, o);
}

/* Always return FALSE because its not interactive */
static gboolean run_func(ObActionsData *data, gpointer options)
{
    Options *o = options;

    if (o->str) g_print("%s\n", o->str);

    return FALSE;
}
