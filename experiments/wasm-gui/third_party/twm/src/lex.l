%{

/*****************************************************************************/
/*

Copyright 1989,1998  The Open Group

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
/**       Copyright 1988 by Evans & Sutherland Computer Corporation,        **/
/**                          Salt Lake City, Utah                           **/
/**                        Cambridge, Massachusetts                         **/
/**                                                                         **/
/**                           All Rights Reserved                           **/
/**                                                                         **/
/**    Permission to use, copy, modify, and distribute this software and    **/
/**    its documentation  for  any  purpose  and  without  fee is hereby    **/
/**    granted, provided that the above copyright notice appear  in  all    **/
/**    copies and that both  that  copyright  notice  and  this  permis-    **/
/**    sion  notice appear in supporting  documentation,  and  that  the    **/
/**    name of Evans & Sutherland not be used in advertising    **/
/**    in publicity pertaining to distribution of the  software  without    **/
/**    specific, written prior permission.                                  **/
/**                                                                         **/
/**    EVANS & SUTHERLAND DISCLAIMs ALL WARRANTIES WITH REGARD    **/
/**    TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES  OF  MERCHANT-    **/
/**    ABILITY  AND  FITNESS,  IN  NO  EVENT SHALL EVANS & SUTHERLAND    **/
/**    BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL  DAM-    **/
/**    AGES OR  ANY DAMAGES WHATSOEVER  RESULTING FROM LOSS OF USE, DATA    **/
/**    OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER    **/
/**    TORTIOUS ACTION, ARISING OUT OF OR IN  CONNECTION  WITH  THE  USE    **/
/**    OR PERFORMANCE OF THIS SOFTWARE.                                     **/
/*****************************************************************************/

/***********************************************************************
 *
 * $Xorg: lex.l,v 1.4 2001/02/09 02:05:36 xorgcvs Exp $
 *
 * .twmrc lex file
 *
 * 12-Nov-87 Thomas E. LaStrange                File created
 *
 ***********************************************************************/
/* $XFree86: xc/programs/twm/lex.l,v 3.13 2001/08/27 21:11:39 dawes Exp $ */

#define YY_NO_INPUT	/* we have our own! */

/* #include <stdio.h> */ /* lex already includes stdio.h */
#include "twm.h"
#include "gram.h"
#include "list.h"
#include "parse.h"

#ifdef FLEX_SCANNER
int yylineno;

#undef YY_INPUT
#define YY_INPUT(buf,result,size) ((result) = doinput((buf),(size)))
static int
doinput(char *buf, int size)
{
    int c;

    if (size == 0)
        return (0);

    if ((c = (*twmInputFunc) ()) <= 0)
        return (0);

    buf[0] = (char) c;
    if (c == '\n')
        ++yylineno;
    return (1);
}
#define YY_NO_UNPUT
#endif

%}

string                          \"([^"]|\\.)*\"
number                          [0-9]+
%%
"{"                             { return (LB); }
"}"                             { return (RB); }
"("                             { return (LP); }
")"                             { return (RP); }
"="                             { return (EQUALS); }
":"                             { return (COLON); }
"+"                             { return PLUS; }
"-"                             { return MINUS; }
"|"                             { return OR; }

[a-zA-Z\.]+                     { int token = parse_keyword ((char *)yytext,
                                                             &yylval.num);
                                  if (token == ERRORTOKEN) {
                                      parseWarning (
                                           "ignoring unknown keyword: %s",
                                           yytext);
                                      ParseError = 1;
                                  } else
                                    return token;
                                }

"!"                             { yylval.num = F_EXEC; return FSKEYWORD; }
"^"                             { yylval.num = F_CUT; return FSKEYWORD; }

{string}                        { yylval.ptr = (char *)yytext; return STRING; }
{number}                        { (void)sscanf((char *)yytext, "%d", &yylval.num);
                                  return (NUMBER);
                                }
\#[^\n]*\n                      {;}
[\r\n\t ]                       {;}
.                               {
                                  parseWarning (
                                       "ignoring character \"%s\"",
                                       yytext);
                                  ParseError = 1;
                                }
%%

#ifndef yywrap
int
yywrap()
{
    return (1);
}
#endif

#undef unput
#undef input
#undef output
#undef feof
#define unput(c)        twmUnput(c)
#define input()         (*twmInputFunc)()
#define output(c)       TwmOutput(c)
#define feof()          (1)
