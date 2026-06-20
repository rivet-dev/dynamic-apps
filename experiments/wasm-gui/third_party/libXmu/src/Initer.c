/*

Copyright 1988, 1989, 1998  The Open Group

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

*/

/* Created By:  Chris D. Peterson
 *              MIT X Consortium
 * Date:        May 8, 1989
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <X11/Intrinsic.h>
#include <X11/Xmu/Initer.h>

struct InitializerList {
  XmuInitializerProc function;	/* function to call */
  XPointer data;		/* Data to pass the function. */
  XtAppContext * app_con_list;	/* a null terminated list of app_contexts. */
};

/*
 * Prototypes
 */
static Bool AddToAppconList(XtAppContext**, XtAppContext);

static struct InitializerList * init_list = NULL;
static Cardinal init_list_length = 0;

void
XmuAddInitializer(XmuInitializerProc func, XPointer data)
{
  init_list_length++;
  init_list = (struct InitializerList *) XtRealloc( (char *) init_list,
					    (sizeof(struct InitializerList) *
					     init_list_length) );

  init_list[init_list_length - 1].function = func;
  init_list[init_list_length - 1].data = data;
  init_list[init_list_length - 1].app_con_list = NULL;
}

void
XmuCallInitializers(XtAppContext app_con)
{
  unsigned i;

  for (i = 0 ; i < init_list_length ; i++) {
    if (AddToAppconList(&(init_list[i].app_con_list), app_con))
      (init_list[i].function) (app_con, init_list[i].data);
  }
}

/*
 * Function:
 *	AddToAppconList
 *
 * Parameters:
 *	app_list - NULL terminated list of application contexts
 *	app_con	 - application context to test
 *
 * Description:
 *	  Adds an action to the application context list and
 *	returns True, if this app_con is already on the list then
 *	it is NOT added and False is returned.
 *
 * Returns:
 *	True if not found, False if found
 */
static Bool
AddToAppconList(XtAppContext **app_list, XtAppContext app_con)
{
  int i;
  XtAppContext *local_list;

  i = 0;
  local_list = *app_list;
  if (*app_list != NULL) {
    for ( ; *local_list != NULL ; i++, local_list++) {
      if (*local_list == app_con)
	return (False);
    }
  }

  *app_list = (XtAppContext *)  XtRealloc((char *)(*app_list),
					  sizeof(XtAppContext *) * (i + 2) );
  (*app_list)[i++] = app_con;
  (*app_list)[i] = NULL;

  return (True);
}

