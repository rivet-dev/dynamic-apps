/*

Copyright 1988, 1998  The Open Group

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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <X11/Intrinsic.h>
#include "Converters.h"
#include "CharSet.h"

/*
 * Prototypes
 */
static void InitializeQuarks(void);

/*
 * Initialization
 */
static XrmQuark QnotUseful, QwhenMapped, Qalways, Qdefault;
static Boolean haveQuarks;

/*
 * Implementation
 */
static void
InitializeQuarks(void)
{
  if (!haveQuarks)
    {
      char name[11];

      XmuNCopyISOLatin1Lowered(name, XtEnotUseful, sizeof(name));
      QnotUseful = XrmStringToQuark(name);
      XmuNCopyISOLatin1Lowered(name, XtEwhenMapped, sizeof(name));
      QwhenMapped = XrmStringToQuark(name);
      XmuNCopyISOLatin1Lowered(name, XtEalways, sizeof(name));
      Qalways = XrmStringToQuark(name);
      XmuNCopyISOLatin1Lowered(name, XtEdefault, sizeof(name));
      Qdefault = XrmStringToQuark(name);
      haveQuarks = True;
    }
}

/*ARGSUSED*/
void
XmuCvtStringToBackingStore(XrmValue *args, Cardinal *num_args,
			   XrmValuePtr fromVal, XrmValuePtr toVal)
{
    XrmQuark	q;
  char name[11];
    static int	backingStoreType;

    if (*num_args != 0)
        XtWarning("String to BackingStore conversion needs no extra arguments");

  InitializeQuarks();
  XmuNCopyISOLatin1Lowered(name, (char *)fromVal->addr, sizeof(name));

  q = XrmStringToQuark (name);
  if (q == QnotUseful)
	backingStoreType = NotUseful;
  else if (q == QwhenMapped)
    	backingStoreType = WhenMapped;
  else if (q == Qalways)
	backingStoreType = Always;
  else if (q == Qdefault)
    	backingStoreType = Always + WhenMapped + NotUseful;
  else
    {
      XtStringConversionWarning((char *)fromVal->addr, XtRBackingStore);
      return;
    }
  toVal->size = sizeof(int);
  toVal->addr = (XPointer)&backingStoreType;
}

/*ARGSUSED*/
Boolean
XmuCvtBackingStoreToString(Display *dpy, XrmValuePtr args, Cardinal *num_args,
			   XrmValuePtr fromVal, XrmValuePtr toVal,
			   XtPointer *data)
{
  static String buffer;
  Cardinal size;

  switch (*(int *)fromVal->addr)
    {
    case NotUseful:
      buffer = XtEnotUseful;
      break;
    case WhenMapped:
      buffer = XtEwhenMapped;
      break;
    case Always:
      buffer = XtEalways;
      break;
    case (Always + WhenMapped + NotUseful):
      buffer = XtEdefault;
      break;
    default:
      XtWarning("Cannot convert BackingStore to String");
      toVal->addr = NULL;
      toVal->size = 0;
      return (False);
    }

  size = strlen(buffer) + 1;
  if (toVal->addr != NULL)
    {
      if (toVal->size < size)
	{
	  toVal->size = size;
	  return (False);
	}
      strcpy((char *)toVal->addr, buffer);
    }
  else
    toVal->addr = (XPointer)buffer;
  toVal->size = sizeof(String);

  return (True);
}
