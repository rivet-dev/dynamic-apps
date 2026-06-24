#include "openbox/actions.h"
#include "openbox/event.h"
#include "openbox/startupnotify.h"
#include "openbox/client.h"
#include "openbox/prompt.h"
#include "openbox/screen.h"
#include "obt/paths.h"
#include "gettext.h"

#ifdef HAVE_STDLIB_H
#  include <stdlib.h>
#endif

typedef struct {
    gchar   *cmd;
    gboolean sn;
    gchar   *sn_name;
    gchar   *sn_icon;
    gchar   *sn_wmclass;
    gchar   *prompt;
    ObActionsData *data;
} Options;

static gpointer setup_func(xmlNodePtr node);
static void     free_func(gpointer options);
static gboolean run_func(ObActionsData *data, gpointer options);
static void shutdown_func(void);
static void client_dest(ObClient *client, gpointer data);

static GSList *prompt_opts = NULL;

void action_execute_startup(void)
{
    actions_register("Execute", setup_func, free_func, run_func);
    actions_set_shutdown("Execute", shutdown_func);
    actions_set_modifies_focused_window("Execute", FALSE);

    client_add_destroy_notify(client_dest, NULL);
}

static void client_dest(ObClient *client, gpointer data)
{
    GSList *it;

    for (it = prompt_opts; it; it = g_slist_next(it)) {
        Options *o = it->data;
        if (o->data->client == client)
            o->data->client = NULL;
    }
}

static gpointer setup_func(xmlNodePtr node)
{
    xmlNodePtr n;
    Options *o;

    o = g_slice_new0(Options);

    if ((n = obt_xml_find_node(node, "command")) ||
        (n = obt_xml_find_node(node, "execute")))
    {
        gchar *s = obt_xml_node_string(n);
        o->cmd = obt_paths_expand_tilde(s);
        g_free(s);
    }

    if ((n = obt_xml_find_node(node, "prompt")))
        o->prompt = obt_xml_node_string(n);

    if ((n = obt_xml_find_node(node, "startupnotify"))) {
        xmlNodePtr m;
        if ((m = obt_xml_find_node(n->children, "enabled")))
            o->sn = obt_xml_node_bool(m);
        if ((m = obt_xml_find_node(n->children, "name")))
            o->sn_name = obt_xml_node_string(m);
        if ((m = obt_xml_find_node(n->children, "icon")))
            o->sn_icon = obt_xml_node_string(m);
        if ((m = obt_xml_find_node(n->children, "wmclass")))
            o->sn_wmclass = obt_xml_node_string(m);
    }
    return o;
}

static void shutdown_func(void)
{
    client_remove_destroy_notify(client_dest);
}

static void free_func(gpointer options)
{
    Options *o = options;

    if (o) {
        prompt_opts = g_slist_remove(prompt_opts, o);

        g_free(o->cmd);
        g_free(o->sn_name);
        g_free(o->sn_icon);
        g_free(o->sn_wmclass);
        g_free(o->prompt);
        if (o->data) g_slice_free(ObActionsData, o->data);
        g_slice_free(Options, o);
    }
}

static Options* dup_options(Options *in, ObActionsData *data)
{
    Options *o = g_slice_new(Options);
    o->cmd = g_strdup(in->cmd);
    o->sn = in->sn;
    o->sn_name = g_strdup(in->sn_name);
    o->sn_icon = g_strdup(in->sn_icon);
    o->sn_wmclass = g_strdup(in->sn_wmclass);
    o->prompt = NULL;
    o->data = g_slice_new(ObActionsData);
    memcpy(o->data, data, sizeof(ObActionsData));
    return o;
}

static gboolean prompt_cb(ObPrompt *p, gint result, gpointer options)
{
    Options *o = options;
    if (result)
        run_func(o->data, o);
    return TRUE; /* call the cleanup func */
}

static void prompt_cleanup(ObPrompt *p, gpointer options)
{
    prompt_unref(p);
    free_func(options);
}

/* Replace occurrences of $variables */
static gchar* expand_variables(gchar* cmd, ObActionsData* data)
{
    gchar *c, *before, *expand;

    expand = NULL;
    before = cmd;

    while ((c = strchr(before, '$'))) {
        if ((c[1] == 'p' || c[1] == 'P') &&
            (c[2] == 'i' || c[2] == 'I') &&
            (c[3] == 'd' || c[3] == 'D') &&
            !g_ascii_isalnum(c[4]))
        {
            /* found $pid */
            gchar *tmp;

            *c = '\0';
            tmp = expand;
            expand = g_strdup_printf("%s%s%u",
                                     (expand ? expand : ""),
                                     before,
                                     data->client ? data->client->pid : 0);
            g_free(tmp);

            before = c + 4; /* 4 = strlen("$pid") */
        }
        else if ((c[1] == 'w' || c[1] == 'W') &&
                 (c[2] == 'i' || c[2] == 'I') &&
                 (c[3] == 'd' || c[3] == 'D') &&
                 !g_ascii_isalnum(c[4]))
        {
            /* found $wid */
            gchar *tmp;

            *c = '\0';
            tmp = expand;
            expand = g_strdup_printf("%s%s%lu",
                                     (expand ? expand : ""),
                                     before,
                                     data->client ? data->client->window : 0);
            g_free(tmp);

            before = c + 4; /* 4 = strlen("$wid") */
        }
        else if ((c[1] == 'p' || c[1] == 'P') &&
                 (c[2] == 'o' || c[2] == 'O') &&
                 (c[3] == 'i' || c[3] == 'I') &&
                 (c[4] == 'n' || c[4] == 'N') &&
                 (c[5] == 't' || c[5] == 'T') &&
                 (c[6] == 'e' || c[6] == 'E') &&
                 (c[7] == 'r' || c[7] == 'R') &&
                 !g_ascii_isalnum(c[8]))
        {
            /* found $pointer */
            gchar *tmp;

            *c = '\0';
            tmp = expand;
            expand = g_strdup_printf("%s%s%u %u",
                                     (expand ? expand : ""),
                                     before,
                                     data->x, data->y);
            g_free(tmp);

            before = c + 8; /* 4 = strlen("$pointer") */
        }
        else {
            /* found something unknown, copy the $ and continue */
            gchar *tmp;

            *c = '\0';
            tmp = expand;
            expand = g_strdup_printf("%s%s$",
                                     (expand ? expand : ""),
                                     before);
            g_free(tmp);

            before = c + 1; /* 1 = strlen("$") */
        }
    }

    if (expand) {
        gchar *tmp;

        /* add on the end of the string after the last replacement */
        tmp = expand;
        expand = g_strconcat(expand, before, NULL);
        g_free(tmp);

        /* replace the command with the expanded one */
        g_free(cmd);
        cmd = expand;
    }
    return cmd;
}

/* Always return FALSE because its not interactive */
static gboolean run_func(ObActionsData *data, gpointer options)
{
    GError *e;
    gchar **argv = NULL;
    gchar *cmd;
    Options *o = options;

    if (!o->cmd) return FALSE;

    if (o->prompt) {
        ObPrompt *p;
        Options *ocp;
        ObPromptAnswer answers[] = {
            { _("No"), 0 },
            { _("Yes"), 1 }
        };

        ocp = dup_options(options, data);
        p = prompt_new(o->prompt, _("Execute"), answers, 2, 0, 0,
                       prompt_cb, prompt_cleanup, ocp);
        prompt_show(p, NULL, FALSE);

        return FALSE;
    }

    cmd = g_filename_from_utf8(o->cmd, -1, NULL, NULL, NULL);
    if (!cmd) {
        g_message(_("Failed to convert the path \"%s\" from utf8"), o->cmd);
        return FALSE;
    }

    cmd = expand_variables(cmd, data);

    /* If there is a keyboard grab going on then we need to cancel
       it so the application can grab things */
    if (data->uact != OB_USER_ACTION_MENU_SELECTION)
        event_cancel_all_key_grabs();

    e = NULL;
    if (!g_shell_parse_argv(cmd, NULL, &argv, &e)) {
        g_message("%s", e->message);
        g_error_free(e);
    }
    else {
        gchar *program = NULL;
        gboolean ok;

        if (o->sn) {
            program = g_path_get_basename(argv[0]);
            /* sets up the environment */
            sn_setup_spawn_environment(program, o->sn_name, o->sn_icon,
                                       o->sn_wmclass,
                                       /* launch it on the current desktop */
                                       screen_desktop);
        }

        e = NULL;
        ok = g_spawn_async(NULL, argv, NULL,
                           G_SPAWN_SEARCH_PATH |
                           G_SPAWN_DO_NOT_REAP_CHILD,
                           NULL, NULL, NULL, &e);
        if (!ok) {
            g_message("%s", e->message);
            g_error_free(e);
        }

        if (o->sn) {
            if (!ok) sn_spawn_cancel();
            g_unsetenv("DESKTOP_STARTUP_ID");
        }

        g_free(program);
        g_strfreev(argv);
    }

    g_free(cmd);

    return FALSE;
}
