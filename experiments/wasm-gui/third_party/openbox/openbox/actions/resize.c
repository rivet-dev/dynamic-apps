#include "openbox/actions.h"
#include "openbox/moveresize.h"
#include "openbox/client.h"
#include "openbox/frame.h"
#include "openbox/screen.h"
#include "obt/prop.h"

typedef struct {
    gboolean corner_specified;
    guint32 corner;
} Options;

static gpointer setup_func(xmlNodePtr node);
static void free_func(gpointer o);
static gboolean run_func(ObActionsData *data, gpointer options);

static guint32 pick_corner(gint x, gint y, gint cx, gint cy, gint cw, gint ch,
                           gboolean shaded);

void action_resize_startup(void)
{
    actions_register("Resize", setup_func, free_func, run_func);
}

static gpointer setup_func(xmlNodePtr node)
{
    xmlNodePtr n;
    Options *o;

    o = g_slice_new0(Options);

    if ((n = obt_xml_find_node(node, "edge"))) {
        gchar *s = obt_xml_node_string(n);

        o->corner_specified = TRUE;
        if (!g_ascii_strcasecmp(s, "top"))
            o->corner = OBT_PROP_ATOM(NET_WM_MOVERESIZE_SIZE_TOP);
        else if (!g_ascii_strcasecmp(s, "bottom"))
            o->corner = OBT_PROP_ATOM(NET_WM_MOVERESIZE_SIZE_BOTTOM);
        else if (!g_ascii_strcasecmp(s, "left"))
            o->corner = OBT_PROP_ATOM(NET_WM_MOVERESIZE_SIZE_LEFT);
        else if (!g_ascii_strcasecmp(s, "right"))
            o->corner = OBT_PROP_ATOM(NET_WM_MOVERESIZE_SIZE_RIGHT);
        else if (!g_ascii_strcasecmp(s, "topleft"))
            o->corner = OBT_PROP_ATOM(NET_WM_MOVERESIZE_SIZE_TOPLEFT);
        else if (!g_ascii_strcasecmp(s, "topright"))
            o->corner = OBT_PROP_ATOM(NET_WM_MOVERESIZE_SIZE_TOPRIGHT);
        else if (!g_ascii_strcasecmp(s, "bottomleft"))
            o->corner = OBT_PROP_ATOM(NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT);
        else if (!g_ascii_strcasecmp(s, "bottomright"))
            o->corner = OBT_PROP_ATOM(NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT);
        else
            o->corner_specified = FALSE;

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
        guint32 corner;

        if (!data->button)
            corner = OBT_PROP_ATOM(NET_WM_MOVERESIZE_SIZE_KEYBOARD);
        else if (o->corner_specified)
            corner = o->corner; /* it was specified in the binding */
        else
            corner = pick_corner(data->x, data->y,
                                 c->frame->area.x, c->frame->area.y,
                                 /* use the client size because the frame
                                    can be differently sized (shaded
                                    windows) and we want this based on the
                                    clients size */
                                 c->area.width + c->frame->size.left +
                                 c->frame->size.right,
                                 c->area.height + c->frame->size.top +
                                 c->frame->size.bottom, c->shaded);

        moveresize_start(c, data->x, data->y, data->button, corner);
    }

    return FALSE;
}

static guint32 pick_corner(gint x, gint y, gint cx, gint cy, gint cw, gint ch,
                           gboolean shaded)
{
    const Rect *full = screen_physical_area_all_monitors();
    if (cx < full->x) { cw = cw + cx - full->x; cx = full->x; }
    if (cy < full->y) { ch = ch + cy - full->y; cy = full->y; }
    if (cx + cw > full->x + full->width) cw = full->x + full->width - cx;
    if (cy + ch > full->y + full->height) ch = full->y + full->height - cy;

    /* let's make x and y client relative instead of screen relative */
    x = x - cx;
    y = ch - (y - cy); /* y is inverted, 0 is at the bottom of the window */

#define X x*ch/cw
#define A -4*X + 7*ch/3
#define B  4*X -15*ch/9
#define C -X/4 + 2*ch/3
#define D  X/4 + 5*ch/12
#define E  X/4 +   ch/3
#define F -X/4 + 7*ch/12
#define G  4*X - 4*ch/3
#define H -4*X + 8*ch/3
#define a (y > 5*ch/9)
#define b (x < 4*cw/9)
#define c (x > 5*cw/9)
#define d (y < 4*ch/9)

    /*
      Each of these defines (except X which is just there for fun), represents
      the equation of a line. The lines they represent are shown in the diagram
      below. Checking y against these lines, we are able to choose a region
      of the window as shown.

      +---------------------A-------|-------|-------B---------------------+
      |                     |A                     B|                     |
      |                     |A      |       |      B|                     |
      |                     | A                   B |                     |
      |                     | A     |       |     B |                     |
      |                     |  A                 B  |                     |
      |                     |  A    |       |    B  |                     |
      |        northwest    |   A     north     B   |   northeast         |
      |                     |   A   |       |   B   |                     |
      |                     |    A             B    |                     |
      C---------------------+----A--+-------+--B----+---------------------D
      |CCCCCCC              |     A           B     |              DDDDDDD|
      |       CCCCCCCC      |     A |       | B     |      DDDDDDDD       |
      |               CCCCCCC      A         B      DDDDDDD               |
      - - - - - - - - - - - +CCCCCCC+aaaaaaa+DDDDDDD+ - - - - - - - - - - - -
      |                     |       b       c       |                     | sh
      |             west    |       b  move c       |   east              | ad
      |                     |       b       c       |                     | ed
      - - - - - - - - - - - +EEEEEEE+ddddddd+FFFFFFF+- - - - - - - - - - -  -
      |               EEEEEEE      G         H      FFFFFFF               |
      |       EEEEEEEE      |     G |       | H     |      FFFFFFFF       |
      |EEEEEEE              |     G           H     |              FFFFFFF|
      E---------------------+----G--+-------+--H----+---------------------F
      |                     |    G             H    |                     |
      |                     |   G   |       |   H   |                     |
      |        southwest    |   G     south     H   |   southeast         |
      |                     |  G    |       |    H  |                     |
      |                     |  G                 H  |                     |
      |                     | G     |       |     H |                     |
      |                     | G                   H |                     |
      |                     |G      |       |      H|                     |
      |                     |G                     H|                     |
      +---------------------G-------|-------|-------H---------------------+
    */

    if (shaded) {
        /* for shaded windows, you can only resize west/east and move */
        if (b)
            return OBT_PROP_ATOM(NET_WM_MOVERESIZE_SIZE_LEFT);
        if (c)
            return OBT_PROP_ATOM(NET_WM_MOVERESIZE_SIZE_RIGHT);
        return OBT_PROP_ATOM(NET_WM_MOVERESIZE_MOVE);
    }

    if (y < A && y >= C)
        return OBT_PROP_ATOM(NET_WM_MOVERESIZE_SIZE_TOPLEFT);
    else if (y >= A && y >= B && a)
        return OBT_PROP_ATOM(NET_WM_MOVERESIZE_SIZE_TOP);
    else if (y < B && y >= D)
        return OBT_PROP_ATOM(NET_WM_MOVERESIZE_SIZE_TOPRIGHT);
    else if (y < C && y >= E && b)
        return OBT_PROP_ATOM(NET_WM_MOVERESIZE_SIZE_LEFT);
    else if (y < D && y >= F && c)
        return OBT_PROP_ATOM(NET_WM_MOVERESIZE_SIZE_RIGHT);
    else if (y < E && y >= G)
        return OBT_PROP_ATOM(NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT);
    else if (y < G && y < H && d)
        return OBT_PROP_ATOM(NET_WM_MOVERESIZE_SIZE_BOTTOM);
    else if (y >= H && y < F)
        return OBT_PROP_ATOM(NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT);
    else
        return OBT_PROP_ATOM(NET_WM_MOVERESIZE_MOVE);

#undef X
#undef A
#undef B
#undef C
#undef D
#undef E
#undef F
#undef G
#undef H
#undef a
#undef b
#undef c
#undef d
}
