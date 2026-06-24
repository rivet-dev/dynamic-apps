#include <stdlib.h>
#include <stdio.h>
#include <X11/StringDefs.h>
#include <Xaw/Box.h>
#include <Xaw/List.h>
#include <Xaw/Viewport.h>
#include <Xaw/Command.h>

void quit_CB(Widget w, XtPointer closure, XtPointer call_data)
{
   Widget lst=closure;
   XawListReturnStruct *ret;
   
   ret=XawListShowCurrent(lst);

   if (ret->string[0] != 0)
   	printf("selected:%s\n",ret->string);
	   
   XtAppSetExitFlag(XtWidgetToApplicationContext(w));
}

int main(int argc, char **argv)
{
    Widget toplevel,box,list,cmd,view;
    XtAppContext app_con;
    toplevel = XtAppInitialize(&app_con, "Hello", NULL, 0,
                               &argc, argv, NULL,
                               NULL, 0);
    char *items[]={ "the  ","quick","brown", "fox  ",
		    "jumps","Over","the","lazy","dog",
		    NULL};
   
    box = XtCreateManagedWidget("box", boxWidgetClass, toplevel, NULL, 0);
    cmd=XtCreateManagedWidget("exit", commandWidgetClass, box, NULL, 0);

   view=XtVaCreateManagedWidget("viewport", viewportWidgetClass, box,
				XtNallowVert, 1,
                                XtNforceBars,1,
				NULL);
    
   list= XtVaCreateManagedWidget("listbox", listWidgetClass,view,
				  XtNlist,items,
				  XtNdefaultColumns,1,
				  XtNnumberStrings,4,				  
				  XtNforceColumns,1,
				  NULL);
    XawListChange(list,items,0,0,0);
    XtAddCallback(cmd, XtNcallback, quit_CB , list);
    XtRealizeWidget(toplevel);
    XtAppMainLoop(app_con);
    exit(0);
}
