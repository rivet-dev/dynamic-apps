#include "openbox/actions.h"
#include "openbox/client.h"

static gboolean run_func_toggle(ObActionsData *data, gpointer options);

void action_fullscreen_startup(void)
{
    actions_register("ToggleFullscreen", NULL, NULL, run_func_toggle);
}

/* Always return FALSE because its not interactive */
static gboolean run_func_toggle(ObActionsData *data, gpointer options)
{
    if (data->client) {
        actions_client_move(data, TRUE);
        client_fullscreen(data->client, !data->client->fullscreen);
        actions_client_move(data, FALSE);
    }
    return FALSE;
}
