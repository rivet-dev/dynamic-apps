#include "openbox/actions.h"
#include "openbox/focus.h"

static gboolean run_func(ObActionsData *data, gpointer options);

void action_focustobottom_startup(void)
{
    actions_register("FocusToBottom", NULL, NULL, run_func);
}

/* Always return FALSE because its not interactive */
static gboolean run_func(ObActionsData *data, gpointer options)
{
    if (data->client)
        focus_order_to_bottom(data->client);
    return FALSE;
}
