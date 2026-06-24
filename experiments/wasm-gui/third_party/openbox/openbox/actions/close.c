#include "openbox/actions.h"
#include "openbox/client.h"

static gboolean run_func(ObActionsData *data, gpointer options);

void action_close_startup(void)
{
    actions_register("Close",
                     NULL, NULL,
                     run_func);
}

/* Always return FALSE because its not interactive */
static gboolean run_func(ObActionsData *data, gpointer options)
{
    if (data->client) client_close(data->client);

    return FALSE;
}
