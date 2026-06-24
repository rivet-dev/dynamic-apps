#include "openbox/actions.h"
#include "openbox/client.h"

/* These match the values for client_maximize */
typedef enum {
    BOTH = 0,
    HORZ = 1,
    VERT = 2
} MaxDirection;

typedef struct {
    MaxDirection dir;
} Options;

static gpointer setup_func(xmlNodePtr node);
static void free_func(gpointer o);
static gboolean run_func_on(ObActionsData *data, gpointer options);
static gboolean run_func_off(ObActionsData *data, gpointer options);
static gboolean run_func_toggle(ObActionsData *data, gpointer options);
/* 3.4-compatibility */
static gpointer setup_both_func(xmlNodePtr node);
static gpointer setup_horz_func(xmlNodePtr node);
static gpointer setup_vert_func(xmlNodePtr node);

void action_maximize_startup(void)
{
    actions_register("Maximize", setup_func, free_func, run_func_on);
    actions_register("Unmaximize", setup_func, free_func, run_func_off);
    actions_register("ToggleMaximize", setup_func, free_func, run_func_toggle);
    /* 3.4-compatibility */
    actions_register("MaximizeFull", setup_both_func, free_func,
                     run_func_on);
    actions_register("UnmaximizeFull", setup_both_func, free_func,
                     run_func_off);
    actions_register("ToggleMaximizeFull", setup_both_func, free_func,
                     run_func_toggle);
    actions_register("MaximizeHorz", setup_horz_func, free_func,
                     run_func_on);
    actions_register("UnmaximizeHorz", setup_horz_func, free_func,
                     run_func_off);
    actions_register("ToggleMaximizeHorz", setup_horz_func, free_func,
                     run_func_toggle);
    actions_register("MaximizeVert", setup_vert_func, free_func,
                     run_func_on);
    actions_register("UnmaximizeVert", setup_vert_func, free_func,
                     run_func_off);
    actions_register("ToggleMaximizeVert", setup_vert_func, free_func,
                     run_func_toggle);
}

static gpointer setup_func(xmlNodePtr node)
{
    xmlNodePtr n;
    Options *o;

    o = g_slice_new0(Options);
    o->dir = BOTH;

    if ((n = obt_xml_find_node(node, "direction"))) {
        gchar *s = obt_xml_node_string(n);
        if (!g_ascii_strcasecmp(s, "vertical") ||
            !g_ascii_strcasecmp(s, "vert"))
            o->dir = VERT;
        else if (!g_ascii_strcasecmp(s, "horizontal") ||
                 !g_ascii_strcasecmp(s, "horz"))
            o->dir = HORZ;
        g_free(s);
    }

    return o;
}

static void free_func(gpointer o)
{
    g_slice_free(Options, o);
}

/* Always return FALSE because its not interactive */
static gboolean run_func_on(ObActionsData *data, gpointer options)
{
    Options *o = options;
    if (data->client) {
        actions_client_move(data, TRUE);
        client_maximize(data->client, TRUE, o->dir);
        actions_client_move(data, FALSE);
    }
    return FALSE;
}

/* Always return FALSE because its not interactive */
static gboolean run_func_off(ObActionsData *data, gpointer options)
{
    Options *o = options;
    if (data->client) {
        actions_client_move(data, TRUE);
        client_maximize(data->client, FALSE, o->dir);
        actions_client_move(data, FALSE);
    }
    return FALSE;
}

/* Always return FALSE because its not interactive */
static gboolean run_func_toggle(ObActionsData *data, gpointer options)
{
    Options *o = options;
    if (data->client) {
        gboolean toggle;
        actions_client_move(data, TRUE);
        toggle = ((o->dir == HORZ && !data->client->max_horz) ||
                  (o->dir == VERT && !data->client->max_vert) ||
                  (o->dir == BOTH &&
                   !(data->client->max_horz && data->client->max_vert)));
        client_maximize(data->client, toggle, o->dir);
        actions_client_move(data, FALSE);
    }
    return FALSE;
}

/* 3.4-compatibility */
static gpointer setup_both_func(xmlNodePtr node)
{
    Options *o = g_slice_new0(Options);
    o->dir = BOTH;
    return o;
}

static gpointer setup_horz_func(xmlNodePtr node)
{
    Options *o = g_slice_new0(Options);
    o->dir = HORZ;
    return o;
}

static gpointer setup_vert_func(xmlNodePtr node)
{
    Options *o = g_slice_new0(Options);
    o->dir = VERT;
    return o;
}

