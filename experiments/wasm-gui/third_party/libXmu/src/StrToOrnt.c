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
#include <X11/StringDefs.h>
#include "Converters.h"
#include "CharSet.h"

/*
 * Prototypes
 */
static void InitializeQuarks(void);

/*
 * Initialization
 */
static	XrmQuark Qhorizontal, Qvertical;
static Boolean haveQuarks;

/*
 * Implementation
 */
static void
InitializeQuarks(void)
{
  if (!haveQuarks)
    {
      Qhorizontal = XrmPermStringToQuark(XtEhorizontal);
      Qvertical = XrmPermStringToQuark(XtEvertical);
      haveQuarks = True;
    }
}

/*ARGSUSED*/
void
XmuCvtStringToOrientation(XrmValuePtr args, Cardinal *num_args,
			  XrmValuePtr fromVal, XrmValuePtr toVal)
{
    static XtOrientation orient;
    XrmQuark	q;
  char name[11];

  InitializeQuarks();
  XmuNCopyISOLatin1Lowered(name, (char *)fromVal->addr, sizeof(name));
  q = XrmStringToQuark(name);

  toVal->size = sizeof(XtJustify);
  toVal->addr = (XPointer)&orient;

  if (q == Qhorizontal)
    	orient = XtorientHorizontal;
  else if (q == Qvertical)
    	orient = XtorientVertical;
  else
    {
      toVal->addr = NULL;
      XtStringConversionWarning((char *)fromVal->addr, XtROrientation);
    }
}

/*ARGSUSED*/
Boolean
XmuCvtOrientationToString(Display *dpy, XrmValuePtr args, Cardinal *num_args,
			  XrmValuePtr fromVal, XrmValuePtr toVal,
			  XtPointer *data)
{
  static String buffer;
  Cardinal size;

  switch (*(XtOrientation *)fromVal->addr)
    {
    case XtorientVertical:
      buffer = XtEvertical;
      break;
    case XtorientHorizontal:
      buffer = XtEhorizontal;
      break;
    default:
      XtWarning("Cannot convert Orientation to String");
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
