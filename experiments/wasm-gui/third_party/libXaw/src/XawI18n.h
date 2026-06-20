/************************************************************

Copyright 1993, 1994, 1998  The Open Group

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

********************************************************/

#ifdef HAVE_WCTYPE_H
#include <wctype.h>
#ifdef HAVE_WIDEC_H
#include <widec.h>
#define wcslen(c) wslen(c)
#define wcscpy(d, s)		wscpy(d, s)
#define wcsncpy(d, s, l)	wsncpy(d, s, l)
#endif
#endif

#ifdef HAVE_WCHAR_H
#include <wchar.h>
#endif

#include <stdlib.h>

#ifdef USE_XWCHAR_STRING
int _Xwcslen
(
 wchar_t		*wstr
 );

#define wcslen(c) _Xwcslen(c)

wchar_t *_Xwcscpy
(
 wchar_t		*wstr1,
 wchar_t		*wstr2
 );

#define wcscpy(d,s) _Xwcscpy(d,s)

wchar_t *_Xwcsncpy
(
 wchar_t		*wstr1,
 wchar_t		*wstr2,
 int			len
 );

#define wcsncpy(d, s, l)	_Xwcsncpy(d, s, l)

#ifdef USE_XMBTOWC
#define mbtowc(wc, s, l)	_Xmbtowc(wc, s, l)
#endif
#endif

wchar_t _Xaw_atowc
(
#if NeedWidePrototypes
 int			c
#else
 unsigned char		c
#endif
 );

#ifndef HAS_ISW_FUNCS
#  include <ctype.h>
#  ifndef isascii
#    define isascii(c)  ((unsigned char)(c) < 127)
#  endif
#  ifndef toascii
#    define toascii(c)  ((c) & 0x7f)
#  endif
#  ifndef iswspace
#    define iswspace(c) (isascii(c) && isspace(toascii(c)))
#  endif
#endif

#if !defined(iswalnum) && !defined(HAVE_ISWALNUM)
#define iswalnum(c) _Xaw_iswalnum(c)
#endif
int _Xaw_iswalnum
(
 wchar_t		c
 );
