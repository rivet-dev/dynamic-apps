#include "openbox/actions.h"
#include "openbox/client.h"

static gboolean run_func(ObActionsData *data, gpointer options);

void action_iconify_startup(void)
{
    actions_register("Iconify",
                     NULL, NULL,
                     run_func);
}

/* Always return FALSE because its not interactive */
static gboolean run_func(ObActionsData *data, gpointer options)
{
    if (data->client) {
        actions_client_move(data, TRUE);
        client_iconify(data->client, TRUE, TRUE, FALSE);
        actions_client_move(data, FALSE);
    }

    return FALSE;
}
