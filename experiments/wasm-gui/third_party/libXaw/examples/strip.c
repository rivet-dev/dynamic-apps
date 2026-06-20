#include <stdio.h>
#include <stdlib.h>
#include <X11/StringDefs.h>	   
#include <Xaw/Box.h>
#include <Xaw/Command.h>
#include <Xaw/StripChart.h>

static void
quit_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
        XtAppSetExitFlag(XtWidgetToApplicationContext(w));
}

static void
getvalue_cb(Widget widget,XtPointer label_ptr, XtPointer value_ptr)
{
	static double dbl=0,delta=0.1;
	double *ret = (double *)value_ptr;

	dbl=dbl+delta;
	if (dbl >= 1.0 || dbl <= 0 )  { delta = delta*-1.0 ; }
        *ret=dbl;
}

int main(int argc, char **argv)
{
	Widget toplevel,box,chart,command;
	XtAppContext app_con;
	toplevel = XtAppInitialize(&app_con, "demo", NULL, 0,
                               &argc, argv, NULL,
                               NULL, 0);

    box = XtCreateManagedWidget("box", boxWidgetClass, toplevel, NULL, 0);

    command = XtVaCreateManagedWidget("cmd",
                                   commandWidgetClass,  box,
				      XtNlabel, "EXIT",
				      NULL);

    chart = XtVaCreateManagedWidget("chart",
                                   stripChartWidgetClass,  box,
				    XtNupdate, 1,		    
				    NULL);

    XtAddCallback(chart,XtNgetValue,getvalue_cb,NULL);
    XtAddCallback(command, XtNcallback, quit_cb, NULL);
    
    XtRealizeWidget(toplevel);
    XtAppMainLoop(app_con);
    exit(0);
}
