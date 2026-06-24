#include "openbox/actions.h"
#include "openbox/stacking.h"
#include "openbox/window.h"
#include "openbox/event.h"
#include "openbox/focus_cycle.h"
#include "openbox/openbox.h"
#include "gettext.h"
#include "obt/keyboard.h"

typedef struct {
    gboolean linear;
    gboolean dock_windows;
    gboolean desktop_windows;
    gboolean only_hilite_windows;
    gboolean all_desktops;
    gboolean forward;
    gboolean bar;
    gboolean raise;
    gboolean interactive;
    ObFocusCyclePopupMode dialog_mode;
    GSList *actions;


    /* options for after we're done */
    gboolean cancel; /* did the user cancel or not */
    guint state;     /* keyboard state when finished */
} Options;

static gpointer setup_func(xmlNodePtr node,
                           ObActionsIPreFunc *pre,
                           ObActionsIInputFunc *in,
                           ObActionsICancelFunc *c,
                           ObActionsIPostFunc *post);
static gpointer setup_forward_func(xmlNodePtr node,
                                   ObActionsIPreFunc *pre,
                                   ObActionsIInputFunc *in,
                                   ObActionsICancelFunc *c,
                                   ObActionsIPostFunc *post);
static gpointer setup_backward_func(xmlNodePtr node,
                                    ObActionsIPreFunc *pre,
                                    ObActionsIInputFunc *in,
                                    ObActionsICancelFunc *c,
                                    ObActionsIPostFunc *post);
static void     free_func(gpointer options);
static gboolean run_func(ObActionsData *data, gpointer options);
static gboolean i_input_func(guint initial_state,
                             XEvent *e,
                             ObtIC *ic,
                             gpointer options,
                             gboolean *used);
static void     i_cancel_func(gpointer options);
static void     i_post_func(gpointer options);

void action_cyclewindows_startup(void)
{
    actions_register_i("NextWindow", setup_forward_func, free_func, run_func);
    actions_register_i("PreviousWindow", setup_backward_func, free_func,
                       run_func);
}

static gpointer setup_func(xmlNodePtr node,
                           ObActionsIPreFunc *pre,
                           ObActionsIInputFunc *input,
                           ObActionsICancelFunc *cancel,
                           ObActionsIPostFunc *post)
{
    xmlNodePtr n;
    Options *o;

    o = g_slice_new0(Options);
    o->bar = TRUE;
    o->dialog_mode = OB_FOCUS_CYCLE_POPUP_MODE_LIST;
    o->interactive = TRUE;

    if ((n = obt_xml_find_node(node, "linear")))
        o->linear = obt_xml_node_bool(n);
    if ((n = obt_xml_find_node(node, "dialog"))) {
        if (obt_xml_node_contains(n, "none"))
            o->dialog_mode = OB_FOCUS_CYCLE_POPUP_MODE_NONE;
        else if (obt_xml_node_contains(n, "no"))
            o->dialog_mode = OB_FOCUS_CYCLE_POPUP_MODE_NONE;
        else if (obt_xml_node_contains(n, "icons"))
            o->dialog_mode = OB_FOCUS_CYCLE_POPUP_MODE_ICONS;
    }
    if ((n = obt_xml_find_node(node, "interactive")))
        o->interactive = obt_xml_node_bool(n);
    if ((n = obt_xml_find_node(node, "bar")))
        o->bar = obt_xml_node_bool(n);
    if ((n = obt_xml_find_node(node, "raise")))
        o->raise = obt_xml_node_bool(n);
    if ((n = obt_xml_find_node(node, "panels")))
        o->dock_windows = obt_xml_node_bool(n);
    if ((n = obt_xml_find_node(node, "hilite")))
        o->only_hilite_windows = obt_xml_node_bool(n);
    if ((n = obt_xml_find_node(node, "desktop")))
        o->desktop_windows = obt_xml_node_bool(n);
    if ((n = obt_xml_find_node(node, "allDesktops")))
        o->all_desktops = obt_xml_node_bool(n);

    if ((n = obt_xml_find_node(node, "finalactions"))) {
        xmlNodePtr m;

        m = obt_xml_find_node(n->children, "action");
        while (m) {
            ObActionsAct *action = actions_parse(m);
            if (action) o->actions = g_slist_append(o->actions, action);
            m = obt_xml_find_node(m->next, "action");
        }
    }
    else {
        o->actions = g_slist_prepend(o->actions,
                                     actions_parse_string("Focus"));
        o->actions = g_slist_prepend(o->actions,
                                     actions_parse_string("Raise"));
        o->actions = g_slist_prepend(o->actions,
                                     actions_parse_string("Unshade"));
    }

    *input = i_input_func;
    *cancel = i_cancel_func;
    *post = i_post_func;
    return o;
}

static gpointer setup_forward_func(xmlNodePtr node,
                                   ObActionsIPreFunc *pre,
                                   ObActionsIInputFunc *input,
                                   ObActionsICancelFunc *cancel,
                                   ObActionsIPostFunc *post)
{
    Options *o = setup_func(node, pre, input, cancel, post);
    o->forward = TRUE;
    return o;
}

static gpointer setup_backward_func(xmlNodePtr node,
                                    ObActionsIPreFunc *pre,
                                    ObActionsIInputFunc *input,
                                    ObActionsICancelFunc *cancel,
                                    ObActionsIPostFunc *post)
{
    Options *o = setup_func(node, pre, input, cancel, post);
    o->forward = FALSE;
    return o;
}

static void free_func(gpointer options)
{
    Options *o = options;

    while (o->actions) {
        actions_act_unref(o->actions->data);
        o->actions = g_slist_delete_link(o->actions, o->actions);
    }

    g_slice_free(Options, o);
}

static gboolean run_func(ObActionsData *data, gpointer options)
{
    Options *o = options;
    struct _ObClient *ft;

    gboolean done = FALSE;
    gboolean cancel = FALSE;

    ft = focus_cycle(
        o->forward,
        o->all_desktops,
        !o->only_hilite_windows,
        o->dock_windows,
        o->desktop_windows,
        o->linear,
        (o->interactive ? o->bar : FALSE),
        (o->interactive ? o->dialog_mode : OB_FOCUS_CYCLE_POPUP_MODE_NONE),
        done, cancel);

    stacking_restore();
    if (o->raise && ft) stacking_temp_raise(CLIENT_AS_WINDOW(ft));

    return o->interactive;
}

static gboolean i_input_func(guint initial_state,
                             XEvent *e,
                             ObtIC *ic,
                             gpointer options,
                             gboolean *used)
{
    Options *o = options;
    guint mods, initial_mods;

    initial_mods = obt_keyboard_only_modmasks(initial_state);
    mods = obt_keyboard_only_modmasks(e->xkey.state);
    if (e->type == KeyRelease) {
        /* remove from the state the mask of the modifier key being
           released, if it is a modifier key being released that is */
        mods &= ~obt_keyboard_keyevent_to_modmask(e);
    }

    if (e->type == KeyPress) {
        KeySym sym = obt_keyboard_keypress_to_keysym(e);

        /* Escape cancels no matter what */
        if (sym == XK_Escape) {
            o->cancel = TRUE;
            o->state = e->xkey.state;
            return FALSE;
        }

        /* There were no modifiers and they pressed enter */
        else if ((sym == XK_Return || sym == XK_KP_Enter) && !initial_mods) {
            o->cancel = FALSE;
            o->state = e->xkey.state;
            return FALSE;
        }
    }
    /* They released the modifiers */
    else if (e->type == KeyRelease && initial_mods && !(mods & initial_mods))
    {
        o->cancel = FALSE;
        o->state = e->xkey.state;
        return FALSE;
    }

    return TRUE;
}

static void i_cancel_func(gpointer options)
{
    Options *o = options;
    o->cancel = TRUE;
    o->state = 0;
}

static void i_post_func(gpointer options)
{
    Options *o = options;
    struct _ObClient *ft;

    gboolean done = TRUE;

    ft = focus_cycle(o->forward,
                     o->all_desktops,
                     !o->only_hilite_windows,
                     o->dock_windows,
                     o->desktop_windows,
                     o->linear,
                     o->bar,
                     o->dialog_mode,
                     done, o->cancel);

    if (ft)
        actions_run_acts(o->actions, OB_USER_ACTION_KEYBOARD_KEY,
                         o->state, -1, -1, 0, OB_FRAME_CONTEXT_NONE, ft);

    stacking_restore();
}
