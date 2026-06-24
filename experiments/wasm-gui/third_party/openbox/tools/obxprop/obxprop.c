#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <glib.h>

gint fail(const gchar *s) {
    if (s)
        fprintf(stderr, "%s\n", s);
    else
        fprintf
            (stderr,
             "Usage: obxprop [OPTIONS] [--] [PROPERTIES ...]\n\n"
             "Options:\n"
             "    --help              Display this help and exit\n"
             "    --display DISPLAY   Connect to this X display\n"
             "    --id ID             Show the properties for this window\n"
             "    --root              Show the properties for the root window\n");
    return 1;
}

gint parse_hex(gchar *s) {
    gint result = 0;
    while (*s) {
        gint add;
        if (*s >= '0' && *s <='9')
            add = *s-'0';
        else if (*s >= 'A' && *s <='F')
            add = *s-'A'+10;
        else if (*s >= 'a' && *s <='f')
            add = *s-'a'+10;
        else
            break;

        result *= 16;
        result += add;
        ++s;
    }
    return result;
}

Window find_client(Display *d, Window win)
{
    Window r, *children;
    guint n, i;
    Atom state = XInternAtom(d, "WM_STATE", True);
    Atom ret_type;
    gint ret_format, res;
    gulong ret_items, ret_bytesleft, *xdata;

    XQueryTree(d, win, &r, &r, &children, &n);
    for (i = 0; i < n; ++i) {
        Window w = find_client(d, children[i]);
        if (w) return w;
    }

    // try me
    res = XGetWindowProperty(d, win, state, 0, 1,
                       False, state, &ret_type, &ret_format,
                       &ret_items, &ret_bytesleft,
                       (unsigned char**) &xdata);
    XFree(xdata);
    if (res != Success || ret_type == None || ret_items < 1)
        return None;
    return win; // found it!
}

static gboolean get_all(Display *d, Window win, Atom prop,
                        Atom *type, gint *size,
                        guchar **data, guint *num)
{
    gboolean ret = FALSE;
    gint res;
    guchar *xdata = NULL;
    gulong ret_items, bytes_left;

    res = XGetWindowProperty(d, win, prop, 0l, G_MAXLONG,
                             FALSE, AnyPropertyType, type, size,
                             &ret_items, &bytes_left, &xdata);
    if (res == Success) {
        if (ret_items > 0) {
            guint i;

            *data = g_malloc(ret_items * (*size / 8));
            for (i = 0; i < ret_items; ++i)
                switch (*size) {
                case 8:
                    (*data)[i] = xdata[i];
                    break;
                case 16:
                    ((guint16*)*data)[i] = ((gushort*)xdata)[i];
                    break;
                case 32:
                    ((guint32*)*data)[i] = ((gulong*)xdata)[i];
                    break;
                default:
                    g_assert_not_reached(); /* unhandled size */
                }
        }
        *num = ret_items;
        ret = TRUE;
        XFree(xdata);
    }
    return ret;
}

GString *append_string(GString *before, gchar *after, gboolean quote)
{
    const gchar *q = quote ? "\"" : "";
    if (before)
        g_string_append_printf(before, ", %s%s%s", q, after, q);
    else
        g_string_append_printf(before = g_string_new(NULL), "%s%s%s", q, after, q);
    return before;
}

GString *append_int(GString *before, guint after)
{
    if (before)
        g_string_append_printf(before, ", %u", after);
    else
        g_string_append_printf(before = g_string_new(NULL), "%u", after);
    return before;
}

gchar* read_strings(gchar *val, guint n, gboolean utf8)
{
    GSList *strs = NULL, *it;
    GString *ret;
    gchar *p;
    guint i;

    p = val;
    while (p < val + n) {
        strs = g_slist_append(strs, g_strndup(p, n - (p - val)));
        p += strlen(p) + 1; /* next string */
    }

    ret = NULL;
    for (i = 0, it = strs; it; ++i, it = g_slist_next(it)) {
        char *data;

        if (utf8) {
            if (g_utf8_validate(it->data, -1, NULL))
                data = g_strdup(it->data);
            else
                data = g_strdup("");
        }
        else
            data = g_locale_to_utf8(it->data, -1, NULL, NULL, NULL);

        ret = append_string(ret, data, TRUE);
        g_free(data);
    }

    while (strs) {
        g_free(strs->data);
        strs = g_slist_delete_link(strs, strs);
    }
    if (ret)
        return g_string_free(ret, FALSE);
    return NULL;
}

gchar* read_atoms(Display *d, guchar *val, guint n)
{
    GString *ret;
    guint i;

    ret = NULL;
    for (i = 0; i < n; ++i)
        ret = append_string(ret, XGetAtomName(d, ((guint32*)val)[i]), FALSE);
    if (ret)
        return g_string_free(ret, FALSE);
    return NULL;
}

gchar* read_numbers(guchar *val, guint n, guint size)
{
    GString *ret;
    guint i;

    ret = NULL;
    for (i = 0; i < n; ++i)
        switch (size) {
        case 8:
            ret = append_int(ret, ((guint8*)val)[i]);
            break;
        case 16:
            ret = append_int(ret, ((guint16*)val)[i]);
            break;
        case 32:
            ret = append_int(ret, ((guint32*)val)[i]);
            break;
        default:
            g_assert_not_reached(); /* unhandled size */
        }

    if (ret)
        return g_string_free(ret, FALSE);
    return NULL;
}

gboolean read_prop(Display *d, Window w, Atom prop, const gchar **type, gchar **val)
{
    guchar *ret;
    guint nret;
    gint size;
    Atom ret_type;

    ret = NULL;
    if (get_all(d, w, prop, &ret_type, &size, &ret, &nret)) {
        *type = XGetAtomName(d, ret_type);

        if (strcmp(*type, "STRING") == 0)
            *val = read_strings((gchar*)ret, nret, FALSE);
        else if (strcmp(*type, "UTF8_STRING") == 0)
            *val = read_strings((gchar*)ret, nret, TRUE);
        else if (strcmp(*type, "ATOM") == 0) {
            g_assert(size == 32);
            *val = read_atoms(d, ret, nret);
        }
       else
            *val = read_numbers(ret, nret, size);

        g_free(ret);
        return TRUE;
    }
    return FALSE;
}

void show_properties(Display *d, Window w, int argc, char **argv)
{
    Atom* props;
    int i, n;

    props = XListProperties(d, w, &n);

    for (i = 0; i < n; ++i) {
        const char *type;
        char *name, *val;

        name = XGetAtomName(d, props[i]);

        if (read_prop(d, w, props[i], &type, &val)) {
            int found = 1;
            if (argc) {
                int i;

                found = 0;
                for (i = 0; i < argc; i++)
                    if (!strcmp(name, argv[i])) {
                        found = 1;
                        break;
                    }
            }
            if (found)
                g_print("%s(%s) = %s\n", name, type, (val ? val : ""));
            g_free(val);
        }

        XFree(name);
    }

    XFree(props);
}

int main(int argc, char **argv)
{
    Display *d;
    Window id, userid = None;
    int i;
    char *dname = NULL;
    gboolean root = FALSE;

    for (i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--help")) {
            return fail(NULL);
        }
        else if (!strcmp(argv[i], "--root"))
            root = TRUE;
        else if (!strcmp(argv[i], "--id")) {
            if (++i == argc)
                return fail(NULL);
            if (argv[i][0] == '0' && argv[i][1] == 'x') {
                /* hex */
                userid = parse_hex(argv[i]+2);
            }
            else {
                /* decimal */
                userid = atoi(argv[i]);
            }
            if (!userid)
                return fail("Unable to parse argument to --id.");
        }
        else if (!strcmp(argv[i], "--display")) {
            if (++i == argc)
                return fail(NULL);
            dname = argv[i];
        }
        else if (*argv[i] != '-')
            break;
        else if (!strcmp(argv[i], "--")) {
            i++;
            break;
        }
        else
            return fail(NULL);
    }

    d = XOpenDisplay(dname);
    if (!d) {
        return fail("Unable to find an X display. "
                    "Ensure you have permission to connect to the display.");
    }

    if (root)
        userid = RootWindow(d, DefaultScreen(d));

    if (userid == None) {
        int j;
        j = XGrabPointer(d, RootWindow(d, DefaultScreen(d)),
                         False, ButtonPressMask,
                         GrabModeAsync, GrabModeAsync,
                         None, XCreateFontCursor(d, XC_crosshair),
                         CurrentTime);
        if (j != GrabSuccess)
            return fail("Unable to grab the pointer device");
        while (1) {
            XEvent ev;

            XNextEvent(d, &ev);
            if (ev.type == ButtonPress) {
                XUngrabPointer(d, CurrentTime);
                userid = ev.xbutton.subwindow;
                break;
            }
        }
        id = find_client(d, userid);
    }
    else
        id = userid; /* they picked this one */

    if (id == None)
        return fail("Unable to find window with the requested ID");

    show_properties(d, id, argc - i, &argv[i]);
    
    XCloseDisplay(d);

    return 0;
}
