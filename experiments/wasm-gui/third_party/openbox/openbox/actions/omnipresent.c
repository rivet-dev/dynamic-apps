#include "openbox/actions.h"
#include "openbox/client.h"
#include "openbox/screen.h"

static gboolean run_func_toggle(ObActionsData *data, gpointer options);

void action_omnipresent_startup(void)
{
    actions_register("ToggleOmnipresent", NULL, NULL, run_func_toggle);
}

/* Always return FALSE because its not interactive */
static gboolean run_func_toggle(ObActionsData *data, gpointer options)
{
    if (data->client) {
        actions_client_move(data, TRUE);
        client_set_desktop(data->client,
                           data->client->desktop == DESKTOP_ALL ?
                           screen_desktop : DESKTOP_ALL, FALSE, TRUE);
        actions_client_move(data, FALSE);
    }
    return FALSE;
}
