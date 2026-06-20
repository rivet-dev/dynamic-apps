/******************************************************************************

Copyright 1994, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.

Author:  Ralph Mor, X Consortium
******************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <X11/Xos.h>

#ifndef X_NOT_POSIX
#ifdef _POSIX_SOURCE
#include <limits.h>
#else
#define _POSIX_SOURCE
#include <limits.h>
#undef _POSIX_SOURCE
#endif
#endif                          /* X_NOT_POSIX */
#ifndef PATH_MAX
#include <sys/param.h>
#ifndef PATH_MAX
#ifdef MAXPATHLEN
#define PATH_MAX MAXPATHLEN
#else
#define PATH_MAX 1024
#endif
#endif
#endif                          /* PATH_MAX */
#ifdef HAVE_MKSTEMP
#include <unistd.h>
#endif

#include <X11/Xlib.h>
#include <X11/SM/SMlib.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include "twm.h"
#include "screen.h"
#include "session.h"

SmcConn smcConn = NULL;
static XtInputId iceInputId;
static char *twm_clientId;
static TWMWinConfigEntry *winConfigHead = NULL;
static Bool sent_save_done = 0;

#define SAVEFILE_VERSION 2

#ifndef HAVE_MKSTEMP
static char *unique_filename(const char *path, const char *prefix);
#else
static char *unique_filename(const char *path, const char *prefix, int *pFd);
#endif

static char *
GetClientID(Window window)
{
    char *client_id = NULL;
    Window client_leader;
    XTextProperty tp;
    Atom actual_type;
    int actual_format;
    unsigned long nitems;
    unsigned long bytes_after;
    unsigned char *prop = NULL;

    if (XGetWindowProperty(dpy, window, _XA_WM_CLIENT_LEADER,
                           0L, 1L, False, AnyPropertyType, &actual_type,
                           &actual_format, &nitems, &bytes_after,
                           &prop) == Success) {
        if (actual_type == XA_WINDOW && actual_format == 32 && nitems == 1 &&
            bytes_after == 0) {
            client_leader = *((Window *) prop);

            if (XGetTextProperty(dpy, client_leader, &tp, _XA_SM_CLIENT_ID)) {
                if (tp.encoding == XA_STRING &&
                    tp.format == 8 && tp.nitems != 0)
                    client_id = (char *) tp.value;
            }
        }

        if (prop)
            XFree(prop);
    }

    return client_id;
}

static char *
GetWindowRole(Window window)
{
    XTextProperty tp;

    if (XGetTextProperty(dpy, window, &tp, _XA_WM_WINDOW_ROLE)) {
        if (tp.encoding == XA_STRING && tp.format == 8 && tp.nitems != 0)
            return ((char *) tp.value);
    }

    return NULL;
}

static int
write_byte(FILE *file, unsigned char b)
{
    if (fwrite((char *) &b, 1, 1, file) != 1)
        return 0;
    return 1;
}

static int
write_ushort(FILE *file, unsigned short s)
{
    unsigned char file_short[2];

    file_short[0] = (unsigned char) ((s & (unsigned) 0xff00) >> 8);
    file_short[1] = (unsigned char) (s & 0xff);
    if (fwrite((char *) file_short, (int) sizeof(file_short), 1, file) != 1)
        return 0;
    return 1;
}

static int
write_short(FILE *file, short s)
{
    unsigned char file_short[2];

    file_short[0] = (unsigned char) (((unsigned) s & (unsigned) 0xff00) >> 8);
    file_short[1] = (unsigned char) (s & 0xff);
    if (fwrite((char *) file_short, (int) sizeof(file_short), 1, file) != 1)
        return 0;
    return 1;
}

static int
write_counted_string(FILE *file, char *string)
{
    if (string) {
        unsigned char count = (unsigned char) strlen(string);

        if (write_byte(file, count) == 0)
            return 0;
        if (fwrite(string, (int) sizeof(char), (int) count, file) != count)
            return 0;
    }
    else {
        if (write_byte(file, 0) == 0)
            return 0;
    }

    return 1;
}

static int
read_byte(FILE *file, unsigned char *bp)
{
    if (fread((char *) bp, 1, 1, file) != 1)
        return 0;
    return 1;
}

static int
read_ushort(FILE *file, unsigned short *shortp)
{
    unsigned char file_short[2];

    if (fread((char *) file_short, (int) sizeof(file_short), 1, file) != 1)
        return 0;
    *shortp = (unsigned short) (file_short[0] * 256 + file_short[1]);
    return 1;
}

static int
read_short(FILE *file, short *shortp)
{
    unsigned char file_short[2];

    if (fread((char *) file_short, (int) sizeof(file_short), 1, file) != 1)
        return 0;
    *shortp = (short) (file_short[0] * 256 + file_short[1]);
    return 1;
}

static int
read_counted_string(FILE *file, char **stringp)
{
    unsigned char len;
    char *data;

    if (read_byte(file, &len) == 0)
        return 0;
    if (len == 0) {
        data = 0;
    }
    else {
        data = malloc((unsigned) len + 1);
        if (!data)
            return 0;
        if (fread(data, (int) sizeof(char), (int) len, file) != len) {
            free(data);
            return 0;
        }
        data[len] = '\0';
    }
    *stringp = data;
    return 1;
}

/*
 * An entry in the saved window config file looks like this:
 *
 * FIELD                                BYTES
 * -----                                ----
 * SM_CLIENT_ID ID len                  1              (may be 0)
 * SM_CLIENT_ID                         LIST of bytes  (may be NULL)
 *
 * WM_WINDOW_ROLE length                1              (may be 0)
 * WM_WINDOW_ROLE                       LIST of bytes  (may be NULL)
 *
 * if no WM_WINDOW_ROLE (length = 0)
 *
 *   WM_CLASS "res name" length         1
 *   WM_CLASS "res name"                LIST of bytes
 *   WM_CLASS "res class" length        1
 *   WM_CLASS "res class"               LIST of bytes
 *   WM_NAME length                     1               (0 if name changed)
 *   WM_NAME                            LIST of bytes
 *   WM_COMMAND arg count               1               (0 if no SM_CLIENT_ID)
 *   For each arg in WM_COMMAND
 *      arg length                      1
 *      arg                             LIST of bytes
 *
 * Iconified bool                       1
 * Icon info present bool               1
 *
 * if icon info present
 *      icon x                          2
 *      icon y                          2
 *
 * Geom x                               2
 * Geom y                               2
 * Geom width                           2
 * Geom height                          2
 *
 * Width ever changed by user           1
 * Height ever changed by user          1
 */

static int
WriteWinConfigEntry(FILE *configFile, TwmWindow *theWindow,
                    char *clientId, char *windowRole)
{
    char **wm_command;
    int wm_command_count;

    if (!write_counted_string(configFile, clientId))
        return 0;

    if (!write_counted_string(configFile, windowRole))
        return 0;

    if (!windowRole) {
        if (!write_counted_string(configFile, theWindow->class.res_name))
            return 0;
        if (!write_counted_string(configFile, theWindow->class.res_class))
            return 0;
        if (theWindow->nameChanged) {
            /*
             * If WM_NAME changed on this window, we can't use it as
             * a criteria for looking up window configurations.  See the
             * longer explanation in the GetWindowConfig() function below.
             */

            if (!write_counted_string(configFile, NULL))
                return 0;
        }
        else {
            if (!write_counted_string(configFile, theWindow->name))
                return 0;
        }

        wm_command = NULL;
        wm_command_count = 0;
        XGetCommand(dpy, theWindow->w, &wm_command, &wm_command_count);

        if (clientId || !wm_command || wm_command_count == 0) {
            if (!write_byte(configFile, 0))
                return 0;
        }
        else {
            int i;

            if (!write_byte(configFile, (unsigned char) wm_command_count))
                return 0;
            for (i = 0; i < wm_command_count; i++)
                if (!write_counted_string(configFile, wm_command[i]))
                    return 0;
            XFreeStringList(wm_command);
        }
    }

    if (!write_byte(configFile, theWindow->icon ? 1 : 0))       /* iconified */
        return 0;

    if (!write_byte(configFile, theWindow->icon_w ? 1 : 0))     /* icon exists */
        return 0;

    if (theWindow->icon_w) {
        int icon_x, icon_y;

        XGetGeometry(dpy, theWindow->icon_w, &JunkRoot, &icon_x,
                     &icon_y, &JunkWidth, &JunkHeight, &JunkBW, &JunkDepth);

        if (!write_short(configFile, (short) icon_x))
            return 0;
        if (!write_short(configFile, (short) icon_y))
            return 0;
    }

    if (!write_short(configFile, (short) theWindow->frame_x))
        return 0;
    if (!write_short(configFile, (short) theWindow->frame_y))
        return 0;
    if (!write_ushort(configFile, (unsigned short) theWindow->attr.width))
        return 0;
    if (!write_ushort(configFile, (unsigned short) theWindow->attr.height))
        return 0;

    if (!write_byte(configFile, theWindow->widthEverChangedByUser ? 1 : 0))
        return 0;

    if (!write_byte(configFile, theWindow->heightEverChangedByUser ? 1 : 0))
        return 0;

    return 1;
}

static int
ReadWinConfigEntry(FILE *configFile, unsigned short version,
                   TWMWinConfigEntry **pentry)
{
    TWMWinConfigEntry *entry;
    unsigned char byte;
    int i;

    *pentry = entry = malloc(sizeof(TWMWinConfigEntry));
    if (!*pentry)
        return 0;

    entry->tag = 0;
    entry->client_id = NULL;
    entry->window_role = NULL;
    entry->class.res_name = NULL;
    entry->class.res_class = NULL;
    entry->wm_name = NULL;
    entry->wm_command = NULL;
    entry->wm_command_count = 0;

    if (!read_counted_string(configFile, &entry->client_id))
        goto give_up;

    if (!read_counted_string(configFile, &entry->window_role))
        goto give_up;

    if (!entry->window_role) {
        if (!read_counted_string(configFile, &entry->class.res_name))
            goto give_up;
        if (!read_counted_string(configFile, &entry->class.res_class))
            goto give_up;
        if (!read_counted_string(configFile, &entry->wm_name))
            goto give_up;

        if (!read_byte(configFile, &byte))
            goto give_up;
        entry->wm_command_count = byte;

        if (entry->wm_command_count == 0)
            entry->wm_command = NULL;
        else {
            entry->wm_command = malloc((size_t) entry->wm_command_count *
                                       sizeof(char *));

            if (!entry->wm_command)
                goto give_up;

            for (i = 0; i < entry->wm_command_count; i++)
                if (!read_counted_string(configFile, &entry->wm_command[i]))
                    goto give_up;
        }
    }

    if (!read_byte(configFile, &byte))
        goto give_up;
    entry->iconified = byte;

    if (!read_byte(configFile, &byte))
        goto give_up;
    entry->icon_info_present = byte;

    if (entry->icon_info_present) {
        if (!read_short(configFile, (short *) &entry->icon_x))
            goto give_up;
        if (!read_short(configFile, (short *) &entry->icon_y))
            goto give_up;
    }

    if (!read_short(configFile, (short *) &entry->x))
        goto give_up;
    if (!read_short(configFile, (short *) &entry->y))
        goto give_up;
    if (!read_ushort(configFile, &entry->width))
        goto give_up;
    if (!read_ushort(configFile, &entry->height))
        goto give_up;

    if (version > 1) {
        if (!read_byte(configFile, &byte))
            goto give_up;
        entry->width_ever_changed_by_user = byte;

        if (!read_byte(configFile, &byte))
            goto give_up;
        entry->height_ever_changed_by_user = byte;
    }
    else {
        entry->width_ever_changed_by_user = False;
        entry->height_ever_changed_by_user = False;
    }

    return 1;

 give_up:

    if (entry->client_id)
        free(entry->client_id);
    if (entry->window_role)
        free(entry->window_role);
    if (entry->class.res_name)
        free(entry->class.res_name);
    if (entry->class.res_class)
        free(entry->class.res_class);
    if (entry->wm_name)
        free(entry->wm_name);
    if (entry->wm_command_count && entry->wm_command) {
        for (i = 0; i < entry->wm_command_count; i++)
            if (entry->wm_command[i])
                free(entry->wm_command[i]);
    }
    if (entry->wm_command)
        free(entry->wm_command);

    free(entry);
    *pentry = NULL;

    return 0;
}

void
ReadWinConfigFile(char *filename)
{
    FILE *configFile;
    TWMWinConfigEntry *entry;
    int done = 0;
    unsigned short version = 0;

    configFile = fopen(filename, "rb");
    if (!configFile)
        return;

    if (!read_ushort(configFile, &version) || version > SAVEFILE_VERSION) {
        done = 1;
    }

    while (!done) {
        if (ReadWinConfigEntry(configFile, version, &entry)) {
            entry->next = winConfigHead;
            winConfigHead = entry;
        }
        else
            done = 1;
    }

    fclose(configFile);
}

int
GetWindowConfig(TwmWindow *theWindow,
                short *x, short *y,
                unsigned short *width, unsigned short *height,
                Bool *iconified,
                Bool *icon_info_present,
                short *icon_x, short *icon_y,
                Bool *width_ever_changed_by_user,
                Bool *height_ever_changed_by_user)
{
    char *clientId, *windowRole;
    TWMWinConfigEntry *ptr;
    int found = 0;

    ptr = winConfigHead;

    if (!ptr)
        return 0;

    clientId = GetClientID(theWindow->w);
    windowRole = GetWindowRole(theWindow->w);

    while (ptr && !found) {
        int client_id_match = (!clientId && !ptr->client_id) ||
            (clientId && ptr->client_id &&
             strcmp(clientId, ptr->client_id) == 0);

        if (!ptr->tag && client_id_match) {
            if (windowRole || ptr->window_role) {
                found = (windowRole && ptr->window_role &&
                         strcmp(windowRole, ptr->window_role) == 0);
            }
            else {
                /*
                 * Compare WM_CLASS + only compare WM_NAME if the
                 * WM_NAME in the saved file is non-NULL.  If the
                 * WM_NAME in the saved file is NULL, this means that
                 * the client changed the value of WM_NAME during the
                 * session, and we can not use it as a criteria for
                 * our search.  For example, with xmh, at save time
                 * the window name might be "xmh: folderY".  However,
                 * if xmh does not properly restore state when it is
                 * restarted, the initial window name might be
                 * "xmh: folderX".  This would cause the window manager
                 * to fail in finding the saved window configuration.
                 * The best we can do is ignore WM_NAME if its value
                 * changed in the previous session.
                 */

                if (strcmp(theWindow->class.res_name,
                           ptr->class.res_name) == 0 &&
                    strcmp(theWindow->class.res_class,
                           ptr->class.res_class) == 0 &&
                    (ptr->wm_name == NULL ||
                     strcmp(theWindow->name, ptr->wm_name) == 0)) {
                    if (clientId) {
                        /*
                         * If a client ID was present, we should not check
                         * WM_COMMAND because Xt will put a -xtsessionID arg
                         * on the command line.
                         */

                        found = 1;
                    }
                    else {
                        /*
                         * For non-XSMP clients, also check WM_COMMAND.
                         */

                        char **wm_command = NULL;
                        int wm_command_count = 0;

                        XGetCommand(dpy, theWindow->w,
                                    &wm_command, &wm_command_count);

                        if (wm_command_count == ptr->wm_command_count) {
                            int i;

                            for (i = 0; i < wm_command_count; i++)
                                if (strcmp(wm_command[i],
                                           ptr->wm_command[i]) != 0)
                                    break;

                            if (i == wm_command_count)
                                found = 1;
                        }
                    }
                }
            }
        }

        if (!found)
            ptr = ptr->next;
    }

    if (found) {
        *x = ptr->x;
        *y = ptr->y;
        *width = ptr->width;
        *height = ptr->height;
        *iconified = ptr->iconified;
        *icon_info_present = ptr->icon_info_present;
        *width_ever_changed_by_user = ptr->width_ever_changed_by_user;
        *height_ever_changed_by_user = ptr->height_ever_changed_by_user;

        if (*icon_info_present) {
            *icon_x = ptr->icon_x;
            *icon_y = ptr->icon_y;
        }
        ptr->tag = 1;
    }
    else
        *iconified = 0;

    if (clientId)
        XFree(clientId);

    if (windowRole)
        XFree(windowRole);

    return found;
}

#ifndef HAVE_MKSTEMP
static char *
unique_filename(const char *path, const char *prefix)
#else
static char *
unique_filename(const char *path, const char *prefix, int *pFd)
#endif
{
#ifndef HAVE_MKSTEMP
#ifndef X_NOT_POSIX
    return ((char *) tempnam(path, prefix));
#else
    char tempFile[PATH_MAX];
    char *tmp;

    snprintf(tempFile, sizeof(tempFile), "%s/%sXXXXXX", path, prefix);
    tmp = (char *) mktemp(tempFile);
    if (tmp)
        return strdup(tmp);
    else
        return (NULL);
#endif
#else
    char tempFile[PATH_MAX];
    char *ptr;

    snprintf(tempFile, sizeof(tempFile), "%s/%sXXXXXX", path, prefix);
    ptr = strdup(tempFile);
    if (ptr != NULL)
        *pFd = mkstemp(ptr);
    return ptr;
#endif
}

static void
SaveYourselfPhase2CB(SmcConn smcConn2, SmPointer clientData _X_UNUSED)
{
    int scrnum;
    ScreenInfo *theScreen;
    TwmWindow *theWindow;
    char *clientId, *windowRole;
    FILE *configFile = NULL;
    const char *path;
    char *filename = NULL;
    Bool success = False;
    SmProp prop1, prop2, prop3, *props[3];
    SmPropValue prop1val, prop2val, prop3val;
    char discardCommand[80];
    int numVals, i;
    static int first_time = 1;

#ifdef HAVE_MKSTEMP
    int fd;
#endif

    if (first_time) {
        char userId[20];
        char hint = SmRestartIfRunning;

        prop1.name = strdup(SmProgram);
        prop1.type = strdup(SmARRAY8);
        prop1.num_vals = 1;
        prop1.vals = &prop1val;
        prop1val.value = Argv[0];
        prop1val.length = (int) strlen(Argv[0]);

        snprintf(userId, sizeof(userId), "%ld", (long) getuid());
        prop2.name = strdup(SmUserID);
        prop2.type = strdup(SmARRAY8);
        prop2.num_vals = 1;
        prop2.vals = &prop2val;
        prop2val.value = (SmPointer) userId;
        prop2val.length = (int) strlen(userId);

        prop3.name = strdup(SmRestartStyleHint);
        prop3.type = strdup(SmCARD8);
        prop3.num_vals = 1;
        prop3.vals = &prop3val;
        prop3val.value = (SmPointer) & hint;
        prop3val.length = 1;

        props[0] = &prop1;
        props[1] = &prop2;
        props[2] = &prop3;

        SmcSetProperties(smcConn2, 3, props);

        first_time = 0;
    }

    path = getenv("SM_SAVE_DIR");
    if (!path) {
        path = getenv("HOME");
        if (!path)
            path = ".";
    }
#ifndef HAVE_MKSTEMP
    if ((filename = unique_filename(path, ".twm")) == NULL)
        goto bad;

    if (!(configFile = fopen(filename, "wb")))
        goto bad;
#else
    if ((filename = unique_filename(path, ".twm", &fd)) == NULL)
        goto bad;

    if (!(configFile = fdopen(fd, "wb")))
        goto bad;
#endif

    if (!write_ushort(configFile, SAVEFILE_VERSION))
        goto bad;

    success = True;

    for (scrnum = 0; scrnum < NumScreens && success; scrnum++) {
        if (ScreenList[scrnum] != NULL) {
            theScreen = ScreenList[scrnum];
            theWindow = theScreen->TwmRoot.next;

            while (theWindow && success) {
                clientId = GetClientID(theWindow->w);
                windowRole = GetWindowRole(theWindow->w);

                if (!WriteWinConfigEntry(configFile, theWindow,
                                         clientId, windowRole))
                    success = False;

                if (clientId)
                    XFree(clientId);

                if (windowRole)
                    XFree(windowRole);

                theWindow = theWindow->next;
            }
        }
    }

    prop1.name = strdup(SmRestartCommand);
    prop1.type = strdup(SmLISTofARRAY8);

    prop1.vals = malloc((size_t) (Argc + 4) * sizeof(SmPropValue));

    if (!prop1.vals) {
        success = False;
        goto bad;
    }

    numVals = 0;

    for (i = 0; i < Argc; i++) {
        if (strcmp(Argv[i], "-clientId") == 0 ||
            strcmp(Argv[i], "-restore") == 0) {
            i++;
        }
        else {
            prop1.vals[numVals].value = (SmPointer) Argv[i];
            prop1.vals[numVals++].length = (int) strlen(Argv[i]);
        }
    }

    prop1.vals[numVals].value = strdup("-clientId");
    prop1.vals[numVals++].length = 9;

    prop1.vals[numVals].value = strdup(twm_clientId);
    prop1.vals[numVals++].length = (int) strlen(twm_clientId);

    prop1.vals[numVals].value = strdup("-restore");
    prop1.vals[numVals++].length = 8;

    prop1.vals[numVals].value = strdup(filename);
    prop1.vals[numVals++].length = (int) strlen(filename);

    prop1.num_vals = numVals;

    snprintf(discardCommand, sizeof(discardCommand), "rm %s", filename);
    prop2.name = strdup(SmDiscardCommand);
    prop2.type = strdup(SmARRAY8);
    prop2.num_vals = 1;
    prop2.vals = &prop2val;
    prop2val.value = (SmPointer) discardCommand;
    prop2val.length = (int) strlen(discardCommand);

    props[0] = &prop1;
    props[1] = &prop2;

    SmcSetProperties(smcConn2, 2, props);
    free(prop1.vals);

 bad:
    SmcSaveYourselfDone(smcConn2, success);
    sent_save_done = 1;

    if (configFile)
        fclose(configFile);

    if (filename)
        free(filename);
}

static void
SaveYourselfCB(SmcConn smcConn2,
               SmPointer clientData _X_UNUSED,
               int saveType _X_UNUSED,
               Bool shutdown _X_UNUSED,
               int interactStyle _X_UNUSED, Bool fast _X_UNUSED)
{
    if (!SmcRequestSaveYourselfPhase2(smcConn2, SaveYourselfPhase2CB, NULL)) {
        SmcSaveYourselfDone(smcConn2, False);
        sent_save_done = 1;
    }
    else
        sent_save_done = 0;
}

static void
DieCB(SmcConn smcConn2, SmPointer clientData _X_UNUSED)
{
    SmcCloseConnection(smcConn2, 0, NULL);
    XtRemoveInput(iceInputId);
    Done(NULL, NULL);
}

static void
SaveCompleteCB(SmcConn smcConnm _X_UNUSED, SmPointer clientData _X_UNUSED)
{
    ;
}

static void
ShutdownCancelledCB(SmcConn smcConn2, SmPointer clientData _X_UNUSED)
{
    if (!sent_save_done) {
        SmcSaveYourselfDone(smcConn2, False);
        sent_save_done = 1;
    }
}

static void
ProcessIceMsgProc(XtPointer client_data, int *source _X_UNUSED,
                  XtInputId *id _X_UNUSED)
{
    IceConn ice_conn = (IceConn) client_data;

    IceProcessMessages(ice_conn, NULL, NULL);
}

void
ConnectToSessionManager(char *previous_id)
{
    char errorMsg[256];
    unsigned long mask;
    SmcCallbacks callbacks;
    IceConn iceConn;

    mask = SmcSaveYourselfProcMask | SmcDieProcMask |
        SmcSaveCompleteProcMask | SmcShutdownCancelledProcMask;

    callbacks.save_yourself.callback = SaveYourselfCB;
    callbacks.save_yourself.client_data = (SmPointer) NULL;

    callbacks.die.callback = DieCB;
    callbacks.die.client_data = (SmPointer) NULL;

    callbacks.save_complete.callback = SaveCompleteCB;
    callbacks.save_complete.client_data = (SmPointer) NULL;

    callbacks.shutdown_cancelled.callback = ShutdownCancelledCB;
    callbacks.shutdown_cancelled.client_data = (SmPointer) NULL;

    smcConn = SmcOpenConnection(NULL,   /* use SESSION_MANAGER env */
                                (SmPointer) appContext,
                                SmProtoMajor,
                                SmProtoMinor,
                                mask,
                                &callbacks,
                                previous_id, &twm_clientId, 256, errorMsg);

    if (smcConn == NULL)
        return;

    iceConn = SmcGetIceConnection(smcConn);

    iceInputId = XtAppAddInput(appContext,
                               IceConnectionNumber(iceConn),
                               (XtPointer) XtInputReadMask,
                               ProcessIceMsgProc, (XtPointer) iceConn);
}
