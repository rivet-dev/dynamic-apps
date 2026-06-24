#include "openbox/actions.h"
#include "openbox/menu.h"
#include "openbox/place.h"
#include "openbox/geom.h"
#include "openbox/screen.h"
#include "openbox/config.h"
#include <glib.h>

typedef struct {
    gchar         *name;
    GravityPoint   position;
    ObPlaceMonitor monitor_type;
    gint           monitor;
    gboolean       use_position;
} Options;

static gpointer setup_func(xmlNodePtr node);
static void     free_func(gpointer options);
static gboolean run_func(ObActionsData *data, gpointer options);

void action_showmenu_startup(void)
{
    actions_register("ShowMenu", setup_func, free_func, run_func);
}

static gpointer setup_func(xmlNodePtr node)
{
    xmlNodePtr n, c;
    Options *o;
    gboolean x_pos_given = FALSE;

    o = g_slice_new0(Options);
    o->monitor = -1;

    if ((n = obt_xml_find_node(node, "menu")))
        o->name = obt_xml_node_string(n);

    if ((n = obt_xml_find_node(node, "position"))) {
        if ((c = obt_xml_find_node(n->children, "x"))) {
            if (!obt_xml_node_contains(c, "default")) {
                config_parse_gravity_coord(c, &o->position.x);
                x_pos_given = TRUE;
            }
        }

        if (x_pos_given && (c = obt_xml_find_node(n->children, "y"))) {
            if (!obt_xml_node_contains(c, "default")) {
                config_parse_gravity_coord(c, &o->position.y);
                o->use_position = TRUE;
            }
        }

        /* unlike client placement, x/y is needed to specify a monitor,
         * either it's under the mouse or it's in an exact actual position */
        if (o->use_position && (c = obt_xml_find_node(n->children, "monitor"))) {
            if (!obt_xml_node_contains(c, "default")) {
                gchar *s = obt_xml_node_string(c);
                if (!g_ascii_strcasecmp(s, "mouse"))
                    o->monitor_type = OB_PLACE_MONITOR_MOUSE;
                else if (!g_ascii_strcasecmp(s, "active"))
                    o->monitor_type = OB_PLACE_MONITOR_ACTIVE;
                else if (!g_ascii_strcasecmp(s, "primary"))
                    o->monitor_type = OB_PLACE_MONITOR_PRIMARY;
                else if (!g_ascii_strcasecmp(s, "all"))
                    o->monitor_type = OB_PLACE_MONITOR_ALL;
                else
                    o->monitor = obt_xml_node_int(c) - 1;
                g_free(s);
            }
        }
    }
    return o;
}

static void free_func(gpointer options)
{
    Options *o = options;
    g_free(o->name);
    g_slice_free(Options, o);
}

/* Always return FALSE because its not interactive */
static gboolean run_func(ObActionsData *data, gpointer options)
{
    Options *o = options;
    GravityPoint position = { { 0, }, };
    gint monitor = -1;

    if (o->use_position) {
        if (o->monitor >= 0)
            monitor = o->monitor;
        else switch (o->monitor_type) {
            case OB_PLACE_MONITOR_ANY:
            case OB_PLACE_MONITOR_PRIMARY:
                monitor = screen_monitor_primary(FALSE);
                break;
            case OB_PLACE_MONITOR_MOUSE:
                monitor = screen_monitor_pointer();
                break;
            case OB_PLACE_MONITOR_ACTIVE:
                monitor = screen_monitor_active();
                break;
            case OB_PLACE_MONITOR_ALL:
                monitor = screen_num_monitors;
                break;
            default:
                g_assert_not_reached();
        }

        position = o->position;
    } else {
        const Rect *allmon;
        monitor = screen_num_monitors;
        allmon = screen_physical_area_monitor(monitor);
        position.x.pos = data->x - allmon->x;
        position.y.pos = data->y - allmon->y;
    }

    /* you cannot call ShowMenu from inside a menu */
    if (data->uact != OB_USER_ACTION_MENU_SELECTION && o->name)
        menu_show(o->name, &position, monitor,
                  data->button != 0, o->use_position, data->client);

    return FALSE;
}
