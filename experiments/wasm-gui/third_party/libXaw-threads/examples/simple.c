#include <stdlib.h>
#include <X11/StringDefs.h>
#include <Xaw/Box.h>
#include <Xaw/Simple.h>
#include <Xaw/Command.h>
#include <X11/Xmu/Drawing.h>

/*
	this is the  draw function. it uses two pencils (GC) with
	different colors. one pencil is used to draw the logo,
	the other is used to fill the background.
*/

static void ac_set ( Widget w, XEvent *event, String *params, Cardinal *num_params ) 
{
	GC gc1,gc2;
 	XGCValues  values;
	Dimension wi,hi;	

	if (!XtIsRealized(w))
		return ;
	
	if (strcmp(XtName(w),"draw") != 0)
		return ;

	XtVaGetValues(w,XtNheight,&hi,XtNwidth,&wi,NULL);
  	values.foreground   = 0xcf00fe; //purple
  	gc1 = XtGetGC(w, GCForeground, &values);
  	values.foreground    = 0x00ff00; //green
  	gc2 = XtGetGC(w, GCForeground, &values);
	XmuDrawLogo(XtDisplay(w), XtWindow(w), gc1, gc2, 0,0, wi, hi );
	XtReleaseGC(w,gc1);
	XtReleaseGC(w,gc2);
}


static void
quit_cb(Widget w, XtPointer data, XtPointer call_data)
{
     XtAppSetExitFlag( XtWidgetToApplicationContext(w) ); 
};

int main(int argc, char **argv)
{
	Widget toplevel,box,command,simple;
	XtAppContext app;
	static char  translation[] = 
		       "<Expose>:set()\n";
      static XtActionsRec actionTable[] = {
	    {"set",ac_set }
      };

	toplevel = XtAppInitialize(&app, "demo", NULL, 0,
                               &argc, argv, NULL,
                               NULL, 0);

        XtAppAddActions(app, actionTable, XtNumber(actionTable) );

	box = XtCreateManagedWidget("box", boxWidgetClass, toplevel, NULL, 0);

	command = XtVaCreateManagedWidget("cmd",
                                   commandWidgetClass,  box,
                                      XtNlabel, "EXIT",
                                      NULL);

	simple=XtVaCreateManagedWidget("draw",
				       simpleWidgetClass,  box,
				       XtNheight,250,
				       XtNwidth,250,
					XtNbackground, 0xff0000,
				       XtNtranslations,XtParseTranslationTable(translation),
				       NULL);
	
	XtAddCallback(command, XtNcallback, quit_cb,  simple );
	
	XtRealizeWidget(toplevel);
        XtAppMainLoop(app);
	exit(0);
}
