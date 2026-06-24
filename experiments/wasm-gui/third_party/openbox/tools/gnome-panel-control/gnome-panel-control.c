#include <X11/Xlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

typedef enum
{
    NONE,
    MAIN_MENU,
    RUN_DIALOG
} Action;

int main(int argc, char **argv)
{
    int i;
    Action a = NONE;

    for (i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--help")) {
            a = NONE;
            break;
        }
        if (!strcmp(argv[i], "--main-menu")) {
            a = MAIN_MENU;
            break;
        }
        if (!strcmp(argv[i], "--run-dialog")) {
            a = RUN_DIALOG;
            break;
        }
    }

    if (!a) {
        printf("Usage: gnome-panel-control ACTION\n\n");
        printf("Actions:\n");
        printf("    --help       Display this help and exit\n");
        printf("    --main-menu  Show the main menu\n");
        printf("    --run-dialog Show the run dialog\n\n");
        return 0;
    }

    {
        Display *d;
        Window root;
        XClientMessageEvent ce;
        Atom act_atom;
        Time timestamp;

        d = XOpenDisplay(NULL);
        if (!d) {
            fprintf(stderr,
                    "Unable to open the X display specified by the DISPLAY "
                    "environment variable. Ensure you have permission to "
                    "connect to the display.");
            return 1;
        }
        root = RootWindowOfScreen(DefaultScreenOfDisplay(d));

        switch (a) {
        case MAIN_MENU:
            act_atom = XInternAtom(d, "_GNOME_PANEL_ACTION_MAIN_MENU", False);
            break;
        case RUN_DIALOG:
            act_atom = XInternAtom(d, "_GNOME_PANEL_ACTION_RUN_DIALOG", False);
            break;
        default:
            assert(0);
        }

        /* Generate a timestamp */
        {
            XEvent event;
            Window win;

            win = XCreateSimpleWindow(d, root, 0, 0, 1, 1, 0, 0, 0);

            XSelectInput(d, win, PropertyChangeMask);

            XChangeProperty(d, win, act_atom, act_atom, 8,
                            PropModeAppend, NULL, 0);
            XWindowEvent(d, win, PropertyChangeMask, &event);

            XDestroyWindow(d, win);

            timestamp = event.xproperty.time;
        }

        ce.type = ClientMessage;
        ce.window = root;
        ce.message_type = XInternAtom(d, "_GNOME_PANEL_ACTION", False);
        ce.format = 32;
        ce.data.l[0] = act_atom;
        ce.data.l[1] = timestamp;
        XSendEvent(d, root, False, StructureNotifyMask, (XEvent*) &ce);

        XCloseDisplay(d);
    }

    return 0;
}
