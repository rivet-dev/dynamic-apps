#include <stdlib.h>
#include <stdio.h>
#include <X11/StringDefs.h>
#include <Xaw/Viewport.h>
#include <Xaw/Command.h>
#include <Xaw/Box.h>
static void
quit_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
        XtAppSetExitFlag(XtWidgetToApplicationContext(w));
}

int main(int argc, char **argv)
{
	Widget toplevel,viewport,box,cmd;
    XtAppContext app_con;
    int i;
    char buf[80];
    toplevel = XtAppInitialize(&app_con, "demo", NULL, 0,
                               &argc, argv, NULL,
                               NULL, 0);

    viewport = XtVaCreateManagedWidget("view", viewportWidgetClass, toplevel,
				     XtNforceBars,True,
				       XtNallowVert,True,
				     NULL);

     box = XtVaCreateManagedWidget("box", boxWidgetClass, viewport,
				   XtNvSpace,10,
				   NULL);

     for (i=0;i<10;i++){
	     sprintf(buf,"label%02d",i);
	     cmd=XtVaCreateManagedWidget("cmd",
                                   commandWidgetClass,
				     box, XtNlabel, buf, NULL);
	      XtAddCallback(cmd, XtNcallback, quit_cb, NULL);
     }
     
    XtRealizeWidget(toplevel);


    XtAppMainLoop(app_con);
    exit(0);
}
