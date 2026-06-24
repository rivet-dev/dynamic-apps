#include <stdlib.h>
#include <stdio.h>
#include <X11/StringDefs.h>
#include <Xaw/Box.h>
#include <Xaw/Label.h>
#include <Xaw/Paned.h>
#include <Xaw/Command.h>

static void
quit_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
        XtAppSetExitFlag(XtWidgetToApplicationContext(w));
}

static void
getnumsub_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
	Widget p=XtParent(w);
	char 	buf[80];
	int min,max;
	XawPanedGetMinMax(w,&min,&max);
	
	sprintf(buf,"%d number\n%d min %d max\n",XawPanedGetNumSub(p),min,max);
	XtVaSetValues(w,XtNlabel,buf,NULL);
	
}
static void
refigure_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
		Widget p=XtParent(w);
		XawPanedSetRefigureMode(p,False);
}

int main(int argc, char **argv)
{
	Widget toplevel,pane,label,command;
    XtAppContext app_con;
    toplevel = XtAppInitialize(&app_con, "demo", NULL, 0,
                               &argc, argv, NULL,
                               NULL, 0);

    pane=XtVaCreateManagedWidget( "paned", panedWidgetClass, toplevel,
                                  NULL);

    label = XtVaCreateManagedWidget("Label",
                                   labelWidgetClass,
				    pane, XtNlabel, "quick", NULL);

    XawPanedAllowResize(label,False);
    
    label = XtVaCreateManagedWidget("Label",
                                   labelWidgetClass,
				    pane, XtNlabel, "brown", NULL);

    label = XtVaCreateManagedWidget("Label",
                                   labelWidgetClass,
				    pane, XtNlabel, "fox", NULL);

    command = XtVaCreateManagedWidget("cmd",
                                   commandWidgetClass,  pane,
                                      XtNlabel, "stop resize",
                                      NULL);

     XtAddCallback(command, XtNcallback,refigure_cb, NULL);

    command = XtVaCreateManagedWidget("cmd",
                                   commandWidgetClass,  pane,
                                      XtNlabel, "getnumsub",
                                      NULL);
     
     XtAddCallback(command, XtNcallback,getnumsub_cb, NULL);
    XawPanedSetMinMax(command,50,100);
	
     command = XtVaCreateManagedWidget("cmd",
                                   commandWidgetClass,  pane,
                                      XtNlabel, "EXIT",
                                      NULL);
    XtAddCallback(command, XtNcallback, quit_cb, NULL);

    XtRealizeWidget(toplevel);
    
    XtAppMainLoop(app_con);
    exit(0);
}
