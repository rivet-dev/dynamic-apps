#include "openbox/actions.h"
#include "openbox/client.h"

typedef struct {
    gint layer; /*!< -1 for below, 0 for normal, and 1 for above */
    gboolean toggle;
} Options;

static gpointer setup_func_top(xmlNodePtr node);
static gpointer setup_func_bottom(xmlNodePtr node);
static gpointer setup_func_send(xmlNodePtr node);
static void free_func(gpointer o);
static gboolean run_func(ObActionsData *data, gpointer options);
/* 3.4-compatibility */
static gpointer setup_sendtop_func(xmlNodePtr node);
static gpointer setup_sendbottom_func(xmlNodePtr node);
static gpointer setup_sendnormal_func(xmlNodePtr node);

void action_layer_startup(void)
{
    actions_register("ToggleAlwaysOnTop", setup_func_top, free_func,
                     run_func);
    actions_register("ToggleAlwaysOnBottom", setup_func_bottom, free_func,
                     run_func);
    actions_register("SendToLayer", setup_func_send, free_func,
                     run_func);
    /* 3.4-compatibility */
    actions_register("SendToTopLayer", setup_sendtop_func, free_func,
                     run_func);
    actions_register("SendToBottomLayer", setup_sendbottom_func, free_func,
                     run_func);
    actions_register("SendToNormalLayer", setup_sendnormal_func, free_func,
                     run_func);
}

static gpointer setup_func_top(xmlNodePtr node)
{
    Options *o = g_slice_new0(Options);
    o->layer = 1;
    o->toggle = TRUE;
    return o;
}

static gpointer setup_func_bottom(xmlNodePtr node)
{
    Options *o = g_slice_new0(Options);
    o->layer = -1;
    o->toggle = TRUE;
    return o;
}

static gpointer setup_func_send(xmlNodePtr node)
{
    xmlNodePtr n;
    Options *o;

    o = g_slice_new0(Options);

    if ((n = obt_xml_find_node(node, "layer"))) {
        gchar *s = obt_xml_node_string(n);
        if (!g_ascii_strcasecmp(s, "above") ||
            !g_ascii_strcasecmp(s, "top"))
            o->layer = 1;
        else if (!g_ascii_strcasecmp(s, "below") ||
                 !g_ascii_strcasecmp(s, "bottom"))
            o->layer = -1;
        else if (!g_ascii_strcasecmp(s, "normal") ||
                 !g_ascii_strcasecmp(s, "middle"))
            o->layer = 0;
        g_free(s);
    }

    return o;
}

static void free_func(gpointer o)
{
    g_slice_free(Options, o);
}

/* Always return FALSE because its not interactive */
static gboolean run_func(ObActionsData *data, gpointer options)
{
    Options *o = options;

    if (data->client) {
        ObClient *c = data->client;

        actions_client_move(data, TRUE);

        if (o->layer < 0) {
            if (o->toggle || !c->below)
                client_set_layer(c, c->below ? 0 : -1);
        }
        else if (o->layer > 0) {
            if (o->toggle || !c->above)
                client_set_layer(c, c->above ? 0 : 1);
        }
        else if (c->above || c->below)
            client_set_layer(c, 0);

        actions_client_move(data, FALSE);
    }

    return FALSE;
}

/* 3.4-compatibility */
static gpointer setup_sendtop_func(xmlNodePtr node)
{
    Options *o = g_slice_new0(Options);
    o->layer = 1;
    o->toggle = FALSE;
    return o;
}

static gpointer setup_sendbottom_func(xmlNodePtr node)
{
    Options *o = g_slice_new0(Options);
    o->layer = -1;
    o->toggle = FALSE;
    return o;
}

static gpointer setup_sendnormal_func(xmlNodePtr node)
{
    Options *o = g_slice_new0(Options);
    o->layer = 0;
    o->toggle = FALSE;
    return o;
}

