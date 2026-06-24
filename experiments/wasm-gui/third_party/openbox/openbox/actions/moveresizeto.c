#include "openbox/actions.h"
#include "openbox/client.h"
#include "openbox/screen.h"
#include "openbox/frame.h"
#include "openbox/config.h"

enum {
    CURRENT_MONITOR = -1,
    ALL_MONITORS = -2,
    NEXT_MONITOR = -3,
    PREV_MONITOR = -4
};

typedef struct {
    GravityCoord x;
    GravityCoord y;
    gint w;
    gint w_denom;
    gint h;
    gint h_denom;
    gint monitor;
    gboolean w_sets_client_size;
    gboolean h_sets_client_size;
} Options;

static gpointer setup_func(xmlNodePtr node);
static void free_func(gpointer o);
static gboolean run_func(ObActionsData *data, gpointer options);
/* 3.4-compatibility */
static gpointer setup_center_func(xmlNodePtr node);

void action_moveresizeto_startup(void)
{
    actions_register("MoveResizeTo", setup_func, free_func, run_func);
    /* 3.4-compatibility */
    actions_register("MoveToCenter", setup_center_func, free_func, run_func);
}

static gpointer setup_func(xmlNodePtr node)
{
    xmlNodePtr n;
    Options *o;

    o = g_slice_new0(Options);
    o->x.pos = G_MININT;
    o->y.pos = G_MININT;
    o->w = G_MININT;
    o->h = G_MININT;
    o->monitor = CURRENT_MONITOR;

    if ((n = obt_xml_find_node(node, "x")))
        config_parse_gravity_coord(n, &o->x);

    if ((n = obt_xml_find_node(node, "y")))
        config_parse_gravity_coord(n, &o->y);

    if ((n = obt_xml_find_node(node, "width"))) {
        gchar *s = obt_xml_node_string(n);
        if (g_ascii_strcasecmp(s, "current") != 0)
            config_parse_relative_number(s, &o->w, &o->w_denom);
        g_free(s);

        obt_xml_attr_bool(n, "client", &o->w_sets_client_size);
    }
    if ((n = obt_xml_find_node(node, "height"))) {
        gchar *s = obt_xml_node_string(n);
        if (g_ascii_strcasecmp(s, "current") != 0)
            config_parse_relative_number(s, &o->h, &o->h_denom);
        g_free(s);

        obt_xml_attr_bool(n, "client", &o->h_sets_client_size);
    }

    if ((n = obt_xml_find_node(node, "monitor"))) {
        gchar *s = obt_xml_node_string(n);
        if (g_ascii_strcasecmp(s, "current") != 0) {
            if (!g_ascii_strcasecmp(s, "all"))
                o->monitor = ALL_MONITORS;
            else if(!g_ascii_strcasecmp(s, "next"))
                o->monitor = NEXT_MONITOR;
            else if(!g_ascii_strcasecmp(s, "prev"))
                o->monitor = PREV_MONITOR;
            else
                o->monitor = obt_xml_node_int(n) - 1;
        }
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
        Rect *area, *carea;
        ObClient *c;
        guint mon, cmon;
        gint x, y, lw, lh, w, h;

        c = data->client;
        mon = o->monitor;
        cmon = client_monitor(c);
        switch (mon) {
        case CURRENT_MONITOR:
            mon = cmon; break;
        case ALL_MONITORS:
            mon = SCREEN_AREA_ALL_MONITORS; break;
        case NEXT_MONITOR:
            mon = (cmon + 1 > screen_num_monitors - 1) ? 0 : (cmon + 1); break;
        case PREV_MONITOR:
            mon = (cmon == 0) ? (screen_num_monitors - 1) : (cmon - 1); break;
        default:
            g_assert_not_reached();
        }

        area = screen_area(c->desktop, mon, NULL);
        carea = screen_area(c->desktop, cmon, NULL);

        /* find a target size for the client/frame. */
        w = o->w;
        if (w == G_MININT) { /* not given, so no-op with current value */
            if (o->w_sets_client_size)
                w = c->area.width;
            else
                w = c->frame->area.width;
        }
        else if (o->w_denom) /* used for eg. "1/3" or "55%" */
            w = (w * area->width) / o->w_denom;

        h = o->h;
        if (h == G_MININT) {
            if (o->h_sets_client_size)
                h = c->area.height;
            else
                h = c->frame->area.height;
        }
        else if (o->h_denom)
            h = (h * area->height) / o->h_denom;

        /* get back to the client's size. */
        if (!o->w_sets_client_size)
            w -= c->frame->size.left + c->frame->size.right;
        if (!o->h_sets_client_size)
            h -= c->frame->size.top + c->frame->size.bottom;

        /* it might not be able to resize how they requested, so find out what
           it will actually be resized to */
        x = c->area.x;
        y = c->area.y;
        client_try_configure(c, &x, &y, &w, &h, &lw, &lh, TRUE);

        /* get the frame's size */
        w += c->frame->size.left + c->frame->size.right;
        h += c->frame->size.top + c->frame->size.bottom;

        /* get the position */
        x = o->x.pos;
        if (o->x.denom) /* relative positions */
            x = (x * area->width) / o->x.denom;
        if (o->x.center) x = (area->width - w) / 2;
        else if (x == G_MININT) /* not specified */
            x = c->frame->area.x - carea->x;
        else if (o->x.opposite) /* value relative to right edge instead of left */
            x = area->width - w - x;
        x += area->x;

        y = o->y.pos;
        if (o->y.denom)
            y = (y * area->height) / o->y.denom;
        if (o->y.center) y = (area->height - h) / 2;
        else if (y == G_MININT)
            y = c->frame->area.y - carea->y;
        else if (o->y.opposite)
            y = area->height - h - y;
        y += area->y;

        /* get the client's size back */
        w -= c->frame->size.left + c->frame->size.right;
        h -= c->frame->size.top + c->frame->size.bottom;

        frame_frame_gravity(c->frame, &x, &y); /* get the client coords */
        client_try_configure(c, &x, &y, &w, &h, &lw, &lh, TRUE);
        /* force it on screen if its moving to another monitor */
        client_find_onscreen(c, &x, &y, w, h, mon != cmon);

        actions_client_move(data, TRUE);
        client_configure(c, x, y, w, h, TRUE, TRUE, FALSE);
        actions_client_move(data, FALSE);

        g_slice_free(Rect, area);
        g_slice_free(Rect, carea);
    }

    return FALSE;
}

/* 3.4-compatibility */
static gpointer setup_center_func(xmlNodePtr node)
{
    Options *o;

    o = g_slice_new0(Options);
    o->x.pos = G_MININT;
    o->y.pos = G_MININT;
    o->w = G_MININT;
    o->h = G_MININT;
    o->monitor = CURRENT_MONITOR;
    o->x.center = TRUE;
    o->y.center = TRUE;
    return o;
}
