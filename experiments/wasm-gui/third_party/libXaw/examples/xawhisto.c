#include <stdio.h>
#include <stdlib.h>

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/Scrollbar.h>


static XtAppContext app_context;
static Widget scrollbar;
static Widget command;
static XtIntervalId timerId;

static void update(XtPointer client_data, XtIntervalId * id)
{
    char buf[28];
    static int i = 1;

    sprintf(buf, "% 2d%%", i);
    XtVaSetValues(command, XtNlabel, buf, NULL);
    XawScrollbarSetThumb(scrollbar, 0.0, (i / 100.0));
    i ++;
    i %= 100;
    timerId = XtAppAddTimeOut(app_context, 500 , update, app_context);
}

static void cmd_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
	XtAppSetExitFlag(XtWidgetToApplicationContext(w));
}

int main(int argc, char **argv)
{

    Widget form;
    Widget topLevel;

    topLevel = XtVaAppInitialize(&app_context, "demo",
				 NULL, 0,
				 &argc, argv, NULL, NULL);

    form = XtVaCreateManagedWidget("form",
				   formWidgetClass, topLevel,
				   XtNorientation, XtorientHorizontal,
				   XtNborderWidth, 0,
				   XtNdefaultDistance, 2,
				   NULL);

    command = XtVaCreateManagedWidget("command",
				      commandWidgetClass, form,
				      XtNleft, XtChainLeft,
				      XtNhighlightThickness, 0,
				      XtNborderWidth, 2,
				      XtNlabel, "start",
				      XtNresize, FALSE,
				      NULL);

    scrollbar = XtVaCreateManagedWidget("scrollbar",
					scrollbarWidgetClass, form,
					XtNhorizDistance, 3,
					XtNfromHoriz, command,
 				        XtNorientation, XtorientHorizontal,
					NULL);

    XawScrollbarSetThumb(scrollbar, 0.0, 0.0);

    /*
     * disable interaction from scrollbar
     */
    XtVaSetValues(scrollbar,
		  XtNtranslations, XtParseTranslationTable(""), NULL);

    XtAddCallback(command, XtNcallback, cmd_cb, NULL);

    XtRealizeWidget(topLevel);
    timerId = XtAppAddTimeOut(app_context, 0, update, app_context);
    XtAppMainLoop(app_context);
    return 0;
}
