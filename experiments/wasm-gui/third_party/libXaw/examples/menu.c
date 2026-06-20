#include <stdlib.h>
#include <stdio.h>
#include <X11/StringDefs.h>
#include <Xaw/Box.h>
#include <Xaw/Command.h>
#include <Xaw/SimpleMenu.h>
#include <Xaw/MenuButton.h>
#include <Xaw/Sme.h>
#include <Xaw/SmeLine.h>
#include <Xaw/SmeBSB.h>

static void
quit_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
        XtAppSetExitFlag(XtWidgetToApplicationContext(w));
}

void setupmenu(Widget top)
{
	Widget button, menu;
        char buf[256];
	int i;

	button = XtVaCreateManagedWidget("menuButton", menuButtonWidgetClass, top,
                                        NULL);
	
	menu = XtVaCreatePopupShell("menu", simpleMenuWidgetClass,
				    button, NULL);

	for (i = 1; i < 6; i++) {
		sprintf(buf, "menuEntry%d", i );
		(void) XtVaCreateManagedWidget(buf, smeBSBObjectClass, menu, NULL);                                
		if (i == 2)
			(void) XtVaCreateManagedWidget("menuButton", smeLineObjectClass, 
                                         menu, NULL);
    }

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

     XtAddCallback(command, XtNcallback, quit_cb, NULL);

     setupmenu(box);
 
    XtRealizeWidget(toplevel);
    XtAppMainLoop(app_con);
    exit(0);
}
