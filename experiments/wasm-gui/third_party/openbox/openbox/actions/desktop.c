#include "openbox/actions.h"
#include "openbox/screen.h"
#include "openbox/client.h"
#include "openbox/openbox.h"
#include "obt/keyboard.h"

typedef enum {
    LAST,
    CURRENT,
    RELATIVE,
    ABSOLUTE
} SwitchType;

typedef struct {
    SwitchType type;
    union {
        struct {
            guint desktop;
        } abs;

        struct {
            gboolean linear;
            gboolean wrap;
            ObDirection dir;
        } rel;
    } u;
    gboolean send;
    gboolean follow;
} Options;

static gpointer setup_func(xmlNodePtr node);
static gpointer setup_send_func(xmlNodePtr node);
static void free_func(gpointer o);
static gboolean run_func(ObActionsData *data, gpointer options);

/* 3.4-compatibility */
static gpointer setup_go_last_func(xmlNodePtr node);
static gpointer setup_send_last_func(xmlNodePtr node);
static gpointer setup_go_abs_func(xmlNodePtr node);
static gpointer setup_go_next_func(xmlNodePtr node);
static gpointer setup_send_next_func(xmlNodePtr node);
static gpointer setup_go_prev_func(xmlNodePtr node);
static gpointer setup_send_prev_func(xmlNodePtr node);
static gpointer setup_go_left_func(xmlNodePtr node);
static gpointer setup_send_left_func(xmlNodePtr node);
static gpointer setup_go_right_func(xmlNodePtr node);
static gpointer setup_send_right_func(xmlNodePtr node);
static gpointer setup_go_up_func(xmlNodePtr node);
static gpointer setup_send_up_func(xmlNodePtr node);
static gpointer setup_go_down_func(xmlNodePtr node);
static gpointer setup_send_down_func(xmlNodePtr node);

void action_desktop_startup(void)
{
    actions_register("GoToDesktop", setup_func, free_func, run_func);
    actions_register("SendToDesktop", setup_send_func, free_func, run_func);
    /* 3.4-compatibility */
    actions_register("DesktopLast", setup_go_last_func, free_func, run_func);
    actions_register("SendToDesktopLast", setup_send_last_func,
                     free_func, run_func);
    actions_register("Desktop", setup_go_abs_func, free_func, run_func);
    actions_register("DesktopNext", setup_go_next_func, free_func, run_func);
    actions_register("SendToDesktopNext", setup_send_next_func,
                     free_func, run_func);
    actions_register("DesktopPrevious", setup_go_prev_func,
                     free_func, run_func);
    actions_register("SendToDesktopPrevious", setup_send_prev_func,
                     free_func, run_func);
    actions_register("DesktopLeft", setup_go_left_func, free_func, run_func);
    actions_register("SendToDesktopLeft", setup_send_left_func,
                     free_func, run_func);
    actions_register("DesktopRight", setup_go_right_func,
                     free_func, run_func);
    actions_register("SendToDesktopRight", setup_send_right_func,
                     free_func, run_func);
    actions_register("DesktopUp", setup_go_up_func, free_func, run_func);
    actions_register("SendToDesktopUp", setup_send_up_func,
                     free_func, run_func);
    actions_register("DesktopDown", setup_go_down_func, free_func, run_func);
    actions_register("SendToDesktopDown", setup_send_down_func,
                     free_func, run_func);
}

static gpointer setup_func(xmlNodePtr node)
{
    xmlNodePtr n;
    Options *o;

    o = g_slice_new0(Options);
    /* don't go anywhere if there are no options given */
    o->type = ABSOLUTE;
    o->u.abs.desktop = screen_desktop;
    /* wrap by default - it's handy! */
    o->u.rel.wrap = TRUE;

    if ((n = obt_xml_find_node(node, "to"))) {
        gchar *s = obt_xml_node_string(n);
        if (!g_ascii_strcasecmp(s, "last"))
            o->type = LAST;
        else if (!g_ascii_strcasecmp(s, "current"))
            o->type = CURRENT;
        else if (!g_ascii_strcasecmp(s, "next")) {
            o->type = RELATIVE;
            o->u.rel.linear = TRUE;
            o->u.rel.dir = OB_DIRECTION_EAST;
        }
        else if (!g_ascii_strcasecmp(s, "previous")) {
            o->type = RELATIVE;
            o->u.rel.linear = TRUE;
            o->u.rel.dir = OB_DIRECTION_WEST;
        }
        else if (!g_ascii_strcasecmp(s, "north") ||
                 !g_ascii_strcasecmp(s, "up")) {
            o->type = RELATIVE;
            o->u.rel.dir = OB_DIRECTION_NORTH;
        }
        else if (!g_ascii_strcasecmp(s, "south") ||
                 !g_ascii_strcasecmp(s, "down")) {
            o->type = RELATIVE;
            o->u.rel.dir = OB_DIRECTION_SOUTH;
        }
        else if (!g_ascii_strcasecmp(s, "west") ||
                 !g_ascii_strcasecmp(s, "left")) {
            o->type = RELATIVE;
            o->u.rel.dir = OB_DIRECTION_WEST;
        }
        else if (!g_ascii_strcasecmp(s, "east") ||
                 !g_ascii_strcasecmp(s, "right")) {
            o->type = RELATIVE;
            o->u.rel.dir = OB_DIRECTION_EAST;
        }
        else {
            o->type = ABSOLUTE;
            o->u.abs.desktop = atoi(s) - 1;
        }
        g_free(s);
    }

    if ((n = obt_xml_find_node(node, "wrap")))
        o->u.rel.wrap = obt_xml_node_bool(n);

    return o;
}

static gpointer setup_send_func(xmlNodePtr node)
{
    xmlNodePtr n;
    Options *o;

    o = setup_func(node);
    if ((n = obt_xml_find_node(node, "desktop"))) {
        /* 3.4 compatibility */
        o->u.abs.desktop = obt_xml_node_int(n) - 1;
        o->type = ABSOLUTE;
    }
    o->send = TRUE;
    o->follow = TRUE;

    if ((n = obt_xml_find_node(node, "follow")))
        o->follow = obt_xml_node_bool(n);

    return o;
}

static void free_func(gpointer o)
{
    g_slice_free(Options, o);
}

static gboolean run_func(ObActionsData *data, gpointer options)
{
    Options *o = options;
    guint d;

    switch (o->type) {
    case LAST:
        d = screen_last_desktop;
        break;
    case CURRENT:
        d = screen_desktop;
        break;
    case ABSOLUTE:
        d = o->u.abs.desktop;
        break;
    case RELATIVE:
        d = screen_find_desktop(screen_desktop,
                                o->u.rel.dir, o->u.rel.wrap, o->u.rel.linear);
        break;
    default:
        g_assert_not_reached();
    }

    if (d < screen_num_desktops &&
        (d != screen_desktop ||
         (data->client && data->client->desktop != screen_desktop))) {
        gboolean go = TRUE;

        actions_client_move(data, TRUE);
        if (o->send && data->client && client_normal(data->client)) {
            client_set_desktop(data->client, d, o->follow, FALSE);
            go = o->follow;
        }

        if (go) {
            screen_set_desktop(d, TRUE);
            if (data->client)
                client_bring_helper_windows(data->client);
        }

        actions_client_move(data, FALSE);
    }

    return FALSE;
}

/* 3.4-compatilibity */
static gpointer setup_follow(xmlNodePtr node)
{
    xmlNodePtr n;
    Options *o = g_slice_new0(Options);
    o->send = TRUE;
    o->follow = TRUE;
    if ((n = obt_xml_find_node(node, "follow")))
        o->follow = obt_xml_node_bool(n);
    return o;
}

static gpointer setup_go_last_func(xmlNodePtr node)
{
    Options *o = g_slice_new0(Options);
    o->type = LAST;
    return o;
}

static gpointer setup_send_last_func(xmlNodePtr node)
{
    Options *o = setup_follow(node);
    o->type = LAST;
    return o;
}

static gpointer setup_go_abs_func(xmlNodePtr node)
{
    xmlNodePtr n;
    Options *o = g_slice_new0(Options);
    o->type = ABSOLUTE;
    if ((n = obt_xml_find_node(node, "desktop")))
        o->u.abs.desktop = obt_xml_node_int(n) - 1;
    else
        o->u.abs.desktop = screen_desktop;
    return o;
}

static void setup_rel(Options *o, xmlNodePtr node, gboolean lin,
                      ObDirection dir)
{
    xmlNodePtr n;

    o->type = RELATIVE;
    o->u.rel.linear = lin;
    o->u.rel.dir = dir;
    o->u.rel.wrap = TRUE;

    if ((n = obt_xml_find_node(node, "wrap")))
        o->u.rel.wrap = obt_xml_node_bool(n);
}

static gpointer setup_go_next_func(xmlNodePtr node)
{
    Options *o = g_slice_new0(Options);
    setup_rel(o, node, TRUE, OB_DIRECTION_EAST);
    return o;
}

static gpointer setup_send_next_func(xmlNodePtr node)
{
    Options *o = setup_follow(node);
    setup_rel(o, node, TRUE, OB_DIRECTION_EAST);
    return o;
}

static gpointer setup_go_prev_func(xmlNodePtr node)
{
    Options *o = g_slice_new0(Options);
    setup_rel(o, node, TRUE, OB_DIRECTION_WEST);
    return o;
}

static gpointer setup_send_prev_func(xmlNodePtr node)
{
    Options *o = setup_follow(node);
    setup_rel(o, node, TRUE, OB_DIRECTION_WEST);
    return o;
}

static gpointer setup_go_left_func(xmlNodePtr node)
{
    Options *o = g_slice_new0(Options);
    setup_rel(o, node, FALSE, OB_DIRECTION_WEST);
    return o;
}

static gpointer setup_send_left_func(xmlNodePtr node)
{
    Options *o = setup_follow(node);
    setup_rel(o, node, FALSE, OB_DIRECTION_WEST);
    return o;
}

static gpointer setup_go_right_func(xmlNodePtr node)
{
    Options *o = g_slice_new0(Options);
    setup_rel(o, node, FALSE, OB_DIRECTION_EAST);
    return o;
}

static gpointer setup_send_right_func(xmlNodePtr node)
{
    Options *o = setup_follow(node);
    setup_rel(o, node, FALSE, OB_DIRECTION_EAST);
    return o;
}

static gpointer setup_go_up_func(xmlNodePtr node)
{
    Options *o = g_slice_new0(Options);
    setup_rel(o, node, FALSE, OB_DIRECTION_NORTH);
    return o;
}

static gpointer setup_send_up_func(xmlNodePtr node)
{
    Options *o = setup_follow(node);
    setup_rel(o, node, FALSE, OB_DIRECTION_NORTH);
    return o;
}

static gpointer setup_go_down_func(xmlNodePtr node)
{
    Options *o = g_slice_new0(Options);
    setup_rel(o, node, FALSE, OB_DIRECTION_SOUTH);
    return o;
}

static gpointer setup_send_down_func(xmlNodePtr node)
{
    Options *o = setup_follow(node);
    setup_rel(o, node, FALSE, OB_DIRECTION_SOUTH);
    return o;
}
