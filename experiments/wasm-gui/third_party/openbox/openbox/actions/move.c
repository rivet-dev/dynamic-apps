#include "openbox/actions.h"
#include "openbox/moveresize.h"
#include "obt/prop.h"

static gboolean run_func(ObActionsData *data, gpointer options);

void action_move_startup(void)
{
    actions_register("Move",
                     NULL, NULL,
                     run_func);
}

/* Always return FALSE because its not interactive */
static gboolean run_func(ObActionsData *data, gpointer options)
{
    if (data->client) {
        guint32 corner;

        corner = data->button != 0 ?
            OBT_PROP_ATOM(NET_WM_MOVERESIZE_MOVE) :
            OBT_PROP_ATOM(NET_WM_MOVERESIZE_MOVE_KEYBOARD);

        moveresize_start(data->client, data->x, data->y, data->button, corner);
    }

    return FALSE;
}
