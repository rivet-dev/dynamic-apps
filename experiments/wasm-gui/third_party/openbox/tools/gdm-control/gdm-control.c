#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <X11/Xlib.h>
#include <X11/Xauth.h>

#include <glib.h>

typedef enum
{
    INVALID,
    NONE,
    SHUTDOWN,
    REBOOT,
    SUSPEND,
    SWITCHUSER
} Action;

#define GDM_PROTOCOL_SOCKET_PATH1 "/var/run/gdm_socket"
#define GDM_PROTOCOL_SOCKET_PATH2 "/tmp/.gdm_socket"

#define GDM_PROTOCOL_MSG_CLOSE         "CLOSE"
#define GDM_PROTOCOL_MSG_VERSION       "VERSION"
#define GDM_PROTOCOL_MSG_AUTHENTICATE  "AUTH_LOCAL"
#define GDM_PROTOCOL_MSG_QUERY_ACTION  "QUERY_LOGOUT_ACTION"
#define GDM_PROTOCOL_MSG_SET_ACTION    "SET_SAFE_LOGOUT_ACTION"
#define GDM_PROTOCOL_MSG_FLEXI_XSERVER "FLEXI_XSERVER"

#define GDM_ACTION_STR_NONE     GDM_PROTOCOL_MSG_SET_ACTION" NONE"
#define GDM_ACTION_STR_SHUTDOWN GDM_PROTOCOL_MSG_SET_ACTION" HALT"
#define GDM_ACTION_STR_REBOOT   GDM_PROTOCOL_MSG_SET_ACTION" REBOOT"
#define GDM_ACTION_STR_SUSPEND  GDM_PROTOCOL_MSG_SET_ACTION" SUSPEND"

#define GDM_MIT_MAGIC_COOKIE_LEN 16

static int fd = 0;

static void gdm_disconnect()
{
    if (fd > 0)
        close(fd);
    fd = 0;
}

static char* get_display_number(void)
{
    char *display_name;
    char *retval;
    char *p;

    display_name = XDisplayName(NULL);

    p = strchr(display_name, ':');
    if (!p)
        return g_strdup ("0");

    while (*p == ':') p++;

    retval = g_strdup (p);

    p = strchr (retval, '.');
    if (p != NULL)
        *p = '\0';

    return retval;
}

static char* gdm_send_protocol_msg (const char *msg)
{
    GString *retval;
    char     buf[256];
    char    *p;
    int      len;

    p = g_strconcat(msg, "\n", NULL);
    if (write (fd, p, strlen(p)) < 0) {
        g_free (p);

        g_warning ("Failed to send message to GDM: %s",
                   g_strerror (errno));
        return NULL;
    }
    g_free (p);

    p = NULL;
    retval = NULL;
    while ((len = read(fd, buf, sizeof(buf) - 1)) > 0) {
        buf[len] = '\0';

        if (!retval)
            retval = g_string_new(buf);
        else
            retval = g_string_append(retval, buf);

        if ((p = strchr(retval->str, '\n')))
            break;
    }

    if (p) *p = '\0';

    return retval ? g_string_free(retval, FALSE) : NULL;
}

static gboolean gdm_authenticate()
{
    FILE       *f;
    Xauth      *xau;
    const char *xau_path;
    char       *display_number;
    gboolean    retval;

    if (!(xau_path = XauFileName()))
        return FALSE;

    if (!(f = fopen(xau_path, "r")))
        return FALSE;

    retval = FALSE;
    display_number = get_display_number();

    while ((xau = XauReadAuth(f))) {
        char  buffer[40]; /* 2*16 == 32, so 40 is enough */
        char *msg;
        char *response;
        int   i;

        if (xau->family != FamilyLocal ||
            strncmp (xau->number, display_number, xau->number_length) ||
            strncmp (xau->name, "MIT-MAGIC-COOKIE-1", xau->name_length) ||
            xau->data_length != GDM_MIT_MAGIC_COOKIE_LEN)
        {
            XauDisposeAuth(xau);
            continue;
        }

        for (i = 0; i < GDM_MIT_MAGIC_COOKIE_LEN; i++)
            g_snprintf(buffer + 2*i, 3, "%02x", (guint)(guchar)xau->data[i]);

        XauDisposeAuth(xau);

        msg = g_strdup_printf(GDM_PROTOCOL_MSG_AUTHENTICATE " %s", buffer);
        response = gdm_send_protocol_msg(msg);
        g_free (msg);

        if (response && !strcmp(response, "OK")) {
            /*auth_cookie = g_strdup(buffer);*/
            g_free(response);
            retval = TRUE;
            break;
        }

        g_free (response);
    }

    fclose(f);
    return retval;
}

static gboolean gdm_connect()
{
    struct sockaddr_un  addr;
    char               *response;

    assert(fd <= 0);

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        g_warning("Failed to create GDM socket: %s", g_strerror (errno));
        gdm_disconnect();
        return FALSE;
    }

    if (g_file_test(GDM_PROTOCOL_SOCKET_PATH1, G_FILE_TEST_EXISTS))
        strcpy(addr.sun_path, GDM_PROTOCOL_SOCKET_PATH1);
    else
        strcpy(addr.sun_path, GDM_PROTOCOL_SOCKET_PATH2);

    addr.sun_family = AF_UNIX;

    if (connect(fd, (struct sockaddr *) &addr, sizeof (addr)) < 0) {
        g_warning("Failed to establish a connection with GDM: %s",
                  g_strerror(errno));
        gdm_disconnect();
        return FALSE;
    }

    response = gdm_send_protocol_msg(GDM_PROTOCOL_MSG_VERSION);
    if (!response || strncmp(response, "GDM ", strlen("GDM ") != 0)) {
        g_free(response);

        g_warning("Failed to get protocol version from GDM");
        gdm_disconnect();
        return FALSE;
    }
    g_free(response);

    if (!gdm_authenticate()) {
        g_warning("Failed to authenticate with GDM");
        gdm_disconnect();
        return FALSE;
    }

    return TRUE;
}

int main(int argc, char **argv)
{
    int i;
    Action a = INVALID;

    for (i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--help")) {
            a = INVALID;
            break;
        }
        if (!strcmp(argv[i], "--none")) {
            a = NONE;
            break;
        }
        if (!strcmp(argv[i], "--shutdown")) {
            a = SHUTDOWN;
            break;
        }
        if (!strcmp(argv[i], "--reboot")) {
            a = REBOOT;
            break;
        }
        if (!strcmp(argv[i], "--suspend")) {
            a = SUSPEND;
            break;
        }
        if (!strcmp(argv[i], "--switch-user")) {
            a = SWITCHUSER;
            break;
        }
    }

    if (!a) {
        printf("Usage: gdm-control ACTION\n\n");
        printf("Actions:\n");
        printf("    --help        Display this help and exit\n");
        printf("    --none        Do nothing special when the current session ends\n");
        printf("    --shutdown    Shutdown the computer when the current session ends\n");
        printf("    --reboot      Reboot the computer when the current session ends\n");
        printf("    --suspend     Suspend the computer when the current session ends\n");
        printf("    --switch-user Log in as a new user (this works immediately)\n\n");
        return 0;
    }

    {
        char *d, *response;
        const char *action_string;

        d = XDisplayName(NULL);
        if (!d) {
            fprintf(stderr,
                    "Unable to find the X display specified by the DISPLAY "
                    "environment variable. Ensure that it is set correctly.");
            return 1;
        }

        switch (a) {
        case NONE:
            action_string = GDM_ACTION_STR_NONE;
            break;
        case SHUTDOWN:
            action_string = GDM_ACTION_STR_SHUTDOWN;
            break;
        case REBOOT:
            action_string = GDM_ACTION_STR_REBOOT;
            break;
        case SUSPEND:
            action_string = GDM_ACTION_STR_SUSPEND;
            break;
        case SWITCHUSER:
            action_string = GDM_PROTOCOL_MSG_FLEXI_XSERVER;
            break;
        default:
            assert(0);
        }

        if (gdm_connect()) {
            response = gdm_send_protocol_msg(action_string);
            g_free(response);
            gdm_disconnect();
        }
    }

    return 0;
}
