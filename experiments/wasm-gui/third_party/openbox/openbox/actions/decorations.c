#include "openbox/actions.h"
#include "openbox/client.h"

static gboolean run_func_on(ObActionsData *data, gpointer options);
static gboolean run_func_off(ObActionsData *data, gpointer options);
static gboolean run_func_toggle(ObActionsData *data, gpointer options);

void action_decorations_startup(void)
{
    actions_register("Decorate", NULL, NULL, run_func_on);
    actions_register("Undecorate", NULL, NULL, run_func_off);
    actions_register("ToggleDecorations", NULL, NULL, run_func_toggle);
}

/* Always return FALSE because its not interactive */
static gboolean run_func_on(ObActionsData *data, gpointer options)
{
    if (data->client) {
        actions_client_move(data, TRUE);
        client_set_undecorated(data->client, FALSE);
        actions_client_move(data, FALSE);
    }
    return FALSE;
}

/* Always return FALSE because its not interactive */
static gboolean run_func_off(ObActionsData *data, gpointer options)
{
    if (data->client) {
        actions_client_move(data, TRUE);
        client_set_undecorated(data->client, TRUE);
        actions_client_move(data, FALSE);
    }
    return FALSE;
}

/* Always return FALSE because its not interactive */
static gboolean run_func_toggle(ObActionsData *data, gpointer options)
{
    if (data->client) {
        actions_client_move(data, TRUE);
        client_set_undecorated(data->client, !data->client->undecorated);
        actions_client_move(data, FALSE);
    }
    return FALSE;
}
