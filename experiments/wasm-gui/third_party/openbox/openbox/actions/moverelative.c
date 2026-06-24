#include "openbox/actions.h"
#include "openbox/client.h"
#include "openbox/screen.h"
#include "openbox/frame.h"
#include "openbox/config.h"

typedef struct {
    gint x;
    gint x_denom;
    gint y;
    gint y_denom;
} Options;

static gpointer setup_func(xmlNodePtr node);
static void free_func(gpointer o);
static gboolean run_func(ObActionsData *data, gpointer options);

void action_moverelative_startup(void)
{
    actions_register("MoveRelative", setup_func, free_func, run_func);
}

static gpointer setup_func(xmlNodePtr node)
{
    xmlNodePtr n;
    Options *o;
    gchar *s;

    o = g_slice_new0(Options);

    if ((n = obt_xml_find_node(node, "x"))) {
        s = obt_xml_node_string(n);
        config_parse_relative_number(s, &o->x, &o->x_denom);
        g_free(s);
    }
    if ((n = obt_xml_find_node(node, "y"))) {
        s = obt_xml_node_string(n);
        config_parse_relative_number(s, &o->y, &o->y_denom);
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
        ObClient *c;
        gint x, y, lw, lh, w, h;

        c = data->client;
        x = o->x;
        y = o->y;
        if (o->x_denom || o->y_denom) {
            const Rect *carea;

            carea = screen_area(c->desktop, client_monitor(c), NULL);
            if (o->x_denom)
                x = (x * carea->width) / o->x_denom;
            if (o->y_denom)
                y = (y * carea->height) / o->y_denom;
        }
        x = c->area.x + x;
        y = c->area.y + y;
        w = c->area.width;
        h = c->area.height;
        client_try_configure(c, &x, &y, &w, &h, &lw, &lh, TRUE);
        client_find_onscreen(c, &x, &y, w, h, FALSE);

        actions_client_move(data, TRUE);
        client_configure(c, x, y, w, h, TRUE, TRUE, FALSE);
        actions_client_move(data, FALSE);
    }

    return FALSE;
}
