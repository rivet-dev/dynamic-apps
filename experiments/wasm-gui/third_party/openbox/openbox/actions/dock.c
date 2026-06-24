#include "openbox/actions.h"
#include "openbox/stacking.h"
#include "openbox/window.h"
#include "openbox/dock.h"

static gboolean raise_func(ObActionsData *data, gpointer options);
static gboolean lower_func(ObActionsData *data, gpointer options);

void action_dock_startup(void)
{
    actions_register("RaiseDock",
                     NULL, NULL,
                     raise_func);
    actions_register("LowerDock",
                     NULL, NULL,
                     lower_func);
}

/* Always return FALSE because its not interactive */
static gboolean raise_func(ObActionsData *data, gpointer options)
{
    actions_client_move(data, TRUE);
    dock_raise_dock();
    actions_client_move(data, FALSE);

    return FALSE;
}

/* Always return FALSE because its not interactive */
static gboolean lower_func(ObActionsData *data, gpointer options)
{
    actions_client_move(data, TRUE);
    dock_lower_dock();
    actions_client_move(data, FALSE);

    return FALSE;
}

