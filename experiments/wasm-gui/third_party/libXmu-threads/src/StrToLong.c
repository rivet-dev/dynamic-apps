/*

Copyright 1989, 1998  The Open Group

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
#include <stdio.h>
#include <X11/Intrinsic.h>
#include <X11/Xmu/SysUtil.h>
#include <X11/Xmu/Converters.h>

void
XmuCvtStringToLong(XrmValuePtr args, Cardinal *num_args,
		   XrmValuePtr fromVal, XrmValuePtr toVal)
{
    static long l;

    if (*num_args != 0)
    XtWarning("String to Long conversion needs no extra arguments");
  if (sscanf((char *)fromVal->addr, "%ld", &l) == 1)
    {
      toVal->size = sizeof(long);
      toVal->addr = (XPointer)&l;
    }
  else
    XtStringConversionWarning((char *)fromVal->addr, XtRLong);
}

/*ARGSUSED*/
Boolean
XmuCvtLongToString(Display *dpy, XrmValuePtr args, Cardinal *num_args,
		   XrmValuePtr fromVal, XrmValuePtr toVal, XtPointer *data)
{
  static char buffer[32];
  size_t size;

  if (*num_args != 0)
    XtWarning("Long to String conversion needs no extra arguments");

  XmuSnprintf(buffer, sizeof(buffer), "%ld", *(long *)fromVal->addr);

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
