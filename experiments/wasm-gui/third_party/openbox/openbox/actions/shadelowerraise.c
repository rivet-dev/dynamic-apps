#include "openbox/actions.h"
#include "openbox/client.h"

static gboolean run_func_sl(ObActionsData *data, gpointer options);
static gboolean run_func_ur(ObActionsData *data, gpointer options);

void action_shadelowerraise_startup()
{
    /* 3.4-compatibility */
    actions_register("ShadeLower", NULL, NULL, run_func_sl);
    actions_register("UnshadeRaise", NULL, NULL, run_func_ur);
}

/* Always return FALSE because its not interactive */
static gboolean run_func_sl(ObActionsData *data, gpointer options)
{
    if (data->client) {
        actions_client_move(data, TRUE);
        if (data->client->shaded)
            stacking_lower(CLIENT_AS_WINDOW(data->client));
        else
            client_shade(data->client, TRUE);
        actions_client_move(data, FALSE);
    }
    return FALSE;
}

/* Always return FALSE because its not interactive */
static gboolean run_func_ur(ObActionsData *data, gpointer options)
{
    if (data->client) {
        actions_client_move(data, TRUE);
        if (data->client->shaded)
            client_shade(data->client, FALSE);
        else
            stacking_raise(CLIENT_AS_WINDOW(data->client));
        actions_client_move(data, FALSE);
    }
    return FALSE;
}
