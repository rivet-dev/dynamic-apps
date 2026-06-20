#include <stdio.h>
#include <stdlib.h>
#include <X11/StringDefs.h>	   
#include <Xaw/Box.h>
#include <Xaw/Command.h>
#include <Xaw/Toggle.h>

static Widget toggle[3];
static Widget    radio_group;

static void
quit_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
        XtAppSetExitFlag(XtWidgetToApplicationContext(w));
}

static void
getdata_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
	char *radio_data=XawToggleGetCurrent(radio_group);
	if (radio_data) 
		printf("radio_data=%s\n",radio_data);
	else
		printf("radio_data=%s\n","nodata");
}

static void
unset_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
	XawToggleUnsetCurrent(radio_group);
}


static void
reset_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
	XawToggleSetCurrent(radio_group,"3397");
}


int main(int argc, char **argv)
{
	Widget toplevel,box,command;
	XtAppContext app_con;

	toplevel = XtAppInitialize(&app_con, "demo", NULL, 0,
                               &argc, argv, NULL,
                               NULL, 0);
	
    box = XtCreateManagedWidget("box", boxWidgetClass, toplevel, NULL, 0);

    command = XtVaCreateManagedWidget("cmd",
                                   commandWidgetClass,  box,
				      XtNlabel, "EXIT",
				      NULL);

    toggle[0]=XtVaCreateManagedWidget("toggle",
				   toggleWidgetClass,  box,
//				   XtNradioData,radioname,
				   XtNradioGroup,radio_group,
   			           XtNlabel, "track",
				   NULL);
    radio_group=toggle[0];
    toggle[1]=XtVaCreateManagedWidget("toggle",
				   toggleWidgetClass,  box,
//				   XtNradioData,radioname,
   				   XtNradioGroup,radio_group,
   			           XtNlabel, "trick",
				    NULL);

    toggle[2]=XtVaCreateManagedWidget("toggle",
				   toggleWidgetClass,  box,
				   XtNradioData,"3397",
				   XtNradioGroup,radio_group,
   			           XtNlabel, "tick",
				    NULL);

    XtAddCallback(command, XtNcallback, quit_cb, NULL);

    command = XtVaCreateManagedWidget("getcurrent",
                                   commandWidgetClass,  box,
				      XtNlabel, "say ",
				      NULL);
      XtAddCallback(command, XtNcallback, getdata_cb, NULL);

      command = XtVaCreateManagedWidget("unsetcurrent",
                                   commandWidgetClass,  box,
				      XtNlabel, "unset ",
				      NULL);
      XtAddCallback(command, XtNcallback, unset_cb, NULL);

      
      command = XtVaCreateManagedWidget("setnew",
                                   commandWidgetClass,  box,
				      XtNlabel, "setnew",
				      NULL);

      XtAddCallback(command, XtNcallback, reset_cb, NULL);
	
    XtRealizeWidget(toplevel);
    XtAppMainLoop(app_con);
    exit(0);
}
