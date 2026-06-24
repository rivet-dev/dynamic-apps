#include "openbox/actions.h"
#include "openbox/openbox.h"
#include "obt/paths.h"

typedef struct {
    gchar   *cmd;
} Options;

static gpointer setup_func(xmlNodePtr node);
static void     free_func(gpointer options);
static gboolean run_func(ObActionsData *data, gpointer options);

void action_restart_startup(void)
{
    actions_register("Restart", setup_func, free_func, run_func);
}

static gpointer setup_func(xmlNodePtr node)
{
    xmlNodePtr n;
    Options *o;

    o = g_slice_new0(Options);

    if ((n = obt_xml_find_node(node, "command")) ||
        (n = obt_xml_find_node(node, "execute")))
    {
        gchar *s = obt_xml_node_string(n);
        o->cmd = obt_paths_expand_tilde(s);
        g_free(s);
    }
    return o;
}

static void free_func(gpointer options)
{
    Options *o = options;
    g_free(o->cmd);
    g_slice_free(Options, o);
}

/* Always return FALSE because its not interactive */
static gboolean run_func(ObActionsData *data, gpointer options)
{
    Options *o = options;

    ob_restart_other(o->cmd);

    return FALSE;
}
