#include "openbox/actions.h"
#include "openbox/stacking.h"
#include "openbox/window.h"

static gboolean run_func(ObActionsData *data, gpointer options);

void action_raise_startup(void)
{
    actions_register("Raise", NULL, NULL, run_func);
}

/* Always return FALSE because its not interactive */
static gboolean run_func(ObActionsData *data, gpointer options)
{
    if (data->client) {
        actions_client_move(data, TRUE);
        stacking_raise(CLIENT_AS_WINDOW(data->client));
        actions_client_move(data, FALSE);
    }

    return FALSE;
}
