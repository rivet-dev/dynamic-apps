#include <stdlib.h>
#include <stdio.h>
#include <X11/StringDefs.h>
#include <Xaw/Box.h>
#include <Xaw/Command.h>
#include <Xaw/Repeater.h>

static int counter;

static void
quit_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
        XtAppSetExitFlag(XtWidgetToApplicationContext(w));
}

static void
start_cb(Widget w, XtPointer ptr, XtPointer arg) 
{
#ifdef DEBUG	
	puts("Start");
#endif
}

static void
do_cb(Widget w, XtPointer ptr, XtPointer arg) 
{
	char buf[10];
	Widget show= (Widget)ptr;
	counter++;
	snprintf(buf,10,"%d",counter);
	XtVaSetValues(show,XtNlabel,buf,NULL);
#ifdef DEBUG
	puts("counter");
#endif
}


static void
stop_cb(Widget w, XtPointer ptr, XtPointer arg) 
{
	counter=0;
#ifdef DEBUG
	puts("reset counter");
#endif
}

int main(int argc, char **argv)
{
  Widget toplevel,box,command,rep;
  XtAppContext app_con;
  toplevel = XtAppInitialize(&app_con, "demo", NULL, 0,
			     &argc, argv, NULL,
			     NULL, 0);

  box = XtCreateManagedWidget("box", boxWidgetClass, toplevel, NULL, 0);
    
  command = XtVaCreateManagedWidget("cmd",
				    commandWidgetClass,  box,
				    XtNlabel, "EXIT",
				    NULL);
  XtAddCallback(command, XtNcallback, quit_cb, NULL);

  rep=XtVaCreateManagedWidget("Repeater",
			      repeaterWidgetClass,
			      box,  XtNlabel,"hitme",NULL);

  XtAddCallback(rep,XtNstartCallback,start_cb,command);

  XtAddCallback(rep,XtNcallback,do_cb,command);

  XtAddCallback(rep,XtNstopCallback,stop_cb,command);
    
  XtRealizeWidget(toplevel);

  XtAppMainLoop(app_con);
  exit(0);
}
