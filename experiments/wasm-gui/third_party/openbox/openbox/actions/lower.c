#include "openbox/actions.h"
#include "openbox/stacking.h"
#include "openbox/window.h"

static gboolean run_func(ObActionsData *data, gpointer options);

void action_lower_startup(void)
{
    actions_register("Lower",
                     NULL, NULL,
                     run_func);
}

/* Always return FALSE because its not interactive */
static gboolean run_func(ObActionsData *data, gpointer options)
{
    if (data->client) {
        actions_client_move(data, TRUE);
        stacking_lower(CLIENT_AS_WINDOW(data->client));
        actions_client_move(data, FALSE);
    }

    return FALSE;
}
