/* A Bison parser, made by GNU Bison 3.0.4.  */

/* Bison interface for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

#ifndef YY_YY_GRAM_H_INCLUDED
# define YY_YY_GRAM_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int yydebug;
#endif

/* Token type.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    LB = 258,
    RB = 259,
    LP = 260,
    RP = 261,
    MENUS = 262,
    MENU = 263,
    BUTTON = 264,
    DEFAULT_FUNCTION = 265,
    PLUS = 266,
    MINUS = 267,
    ALL = 268,
    OR = 269,
    CURSORS = 270,
    PIXMAPS = 271,
    ICONS = 272,
    COLOR = 273,
    SAVECOLOR = 274,
    MONOCHROME = 275,
    FUNCTION = 276,
    ICONMGR_SHOW = 277,
    ICONMGR = 278,
    WINDOW_FUNCTION = 279,
    ZOOM = 280,
    ICONMGRS = 281,
    ICONMGR_GEOMETRY = 282,
    ICONMGR_NOSHOW = 283,
    MAKE_TITLE = 284,
    GRAYSCALE = 285,
    ICONIFY_BY_UNMAPPING = 286,
    DONT_ICONIFY_BY_UNMAPPING = 287,
    NO_TITLE = 288,
    AUTO_RAISE = 289,
    NO_HILITE = 290,
    ICON_REGION = 291,
    META = 292,
    SHIFT = 293,
    LOCK = 294,
    CONTROL = 295,
    WINDOW = 296,
    TITLE = 297,
    ICON = 298,
    ROOT = 299,
    FRAME = 300,
    COLON = 301,
    EQUALS = 302,
    SQUEEZE_TITLE = 303,
    DONT_SQUEEZE_TITLE = 304,
    START_ICONIFIED = 305,
    NO_TITLE_HILITE = 306,
    TITLE_HILITE = 307,
    MOVE = 308,
    RESIZE = 309,
    WAIT = 310,
    SELECT = 311,
    KILL = 312,
    LEFT_TITLEBUTTON = 313,
    RIGHT_TITLEBUTTON = 314,
    NUMBER = 315,
    KEYWORD = 316,
    NKEYWORD = 317,
    CKEYWORD = 318,
    CLKEYWORD = 319,
    FKEYWORD = 320,
    FSKEYWORD = 321,
    SKEYWORD = 322,
    DKEYWORD = 323,
    JKEYWORD = 324,
    WINDOW_RING = 325,
    WARP_CURSOR = 326,
    ERRORTOKEN = 327,
    NO_STACKMODE = 328,
    STRING = 329
  };
#endif
/* Tokens.  */
#define LB 258
#define RB 259
#define LP 260
#define RP 261
#define MENUS 262
#define MENU 263
#define BUTTON 264
#define DEFAULT_FUNCTION 265
#define PLUS 266
#define MINUS 267
#define ALL 268
#define OR 269
#define CURSORS 270
#define PIXMAPS 271
#define ICONS 272
#define COLOR 273
#define SAVECOLOR 274
#define MONOCHROME 275
#define FUNCTION 276
#define ICONMGR_SHOW 277
#define ICONMGR 278
#define WINDOW_FUNCTION 279
#define ZOOM 280
#define ICONMGRS 281
#define ICONMGR_GEOMETRY 282
#define ICONMGR_NOSHOW 283
#define MAKE_TITLE 284
#define GRAYSCALE 285
#define ICONIFY_BY_UNMAPPING 286
#define DONT_ICONIFY_BY_UNMAPPING 287
#define NO_TITLE 288
#define AUTO_RAISE 289
#define NO_HILITE 290
#define ICON_REGION 291
#define META 292
#define SHIFT 293
#define LOCK 294
#define CONTROL 295
#define WINDOW 296
#define TITLE 297
#define ICON 298
#define ROOT 299
#define FRAME 300
#define COLON 301
#define EQUALS 302
#define SQUEEZE_TITLE 303
#define DONT_SQUEEZE_TITLE 304
#define START_ICONIFIED 305
#define NO_TITLE_HILITE 306
#define TITLE_HILITE 307
#define MOVE 308
#define RESIZE 309
#define WAIT 310
#define SELECT 311
#define KILL 312
#define LEFT_TITLEBUTTON 313
#define RIGHT_TITLEBUTTON 314
#define NUMBER 315
#define KEYWORD 316
#define NKEYWORD 317
#define CKEYWORD 318
#define CLKEYWORD 319
#define FKEYWORD 320
#define FSKEYWORD 321
#define SKEYWORD 322
#define DKEYWORD 323
#define JKEYWORD 324
#define WINDOW_RING 325
#define WARP_CURSOR 326
#define ERRORTOKEN 327
#define NO_STACKMODE 328
#define STRING 329

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED

union YYSTYPE
{
#line 109 "gram.y" /* yacc.c:1909  */

    int num;
    char *ptr;

#line 207 "gram.h" /* yacc.c:1909  */
};

typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;

int yyparse (void);

#endif /* !YY_YY_GRAM_H_INCLUDED  */
