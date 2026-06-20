/* A Bison parser, made by GNU Bison 3.0.4.  */

/* Bison implementation for Yacc-like parsers in C

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

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "3.0.4"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1




/* Copy the first part of user declarations.  */
#line 67 "gram.y" /* yacc.c:339  */


#include <stdio.h>
#include <ctype.h>
#include "twm.h"
#include "menus.h"
#include "list.h"
#include "util.h"
#include "screen.h"
#include "parse.h"
#include "add_window.h"
#include "icons.h"
#include <X11/Xos.h>
#include <X11/Xmu/CharSet.h>

static char empty[1];
static char *Action = empty;
static char *Name = empty;
static MenuRoot *root, *pull = NULL;

static MenuRoot *GetRoot(const char *name, const char *fore, const char *back);
static void GotButton(int butt, int func);
static void GotKey(char *key, int func);
static void GotTitleButton(char *bitmapname, int func, Bool rightside);
static Bool CheckWarpScreenArg(char *s);
static Bool CheckWarpRingArg(char *s);
static Bool CheckColormapArg(char *s);
static void RemoveDQuote(char *str);

static char *ptr;
static name_list **list;
static int cont = 0;
static int color;
int mods = 0;
unsigned int mods_used = (ShiftMask | ControlMask | Mod1Mask);

extern int yylineno;
static void yyerror(const char *s);


#line 107 "gram.c" /* yacc.c:339  */

# ifndef YY_NULLPTR
#  if defined __cplusplus && 201103L <= __cplusplus
#   define YY_NULLPTR nullptr
#  else
#   define YY_NULLPTR 0
#  endif
# endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

/* In a future release of Bison, this section will be replaced
   by #include "y.tab.h".  */
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
#line 109 "gram.y" /* yacc.c:355  */

    int num;
    char *ptr;

#line 300 "gram.c" /* yacc.c:355  */
};

typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;

int yyparse (void);

#endif /* !YY_YY_GRAM_H_INCLUDED  */

/* Copy the second part of user declarations.  */

#line 317 "gram.c" /* yacc.c:358  */

#ifdef short
# undef short
#endif

#ifdef YYTYPE_UINT8
typedef YYTYPE_UINT8 yytype_uint8;
#else
typedef unsigned char yytype_uint8;
#endif

#ifdef YYTYPE_INT8
typedef YYTYPE_INT8 yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef YYTYPE_UINT16
typedef YYTYPE_UINT16 yytype_uint16;
#else
typedef unsigned short int yytype_uint16;
#endif

#ifdef YYTYPE_INT16
typedef YYTYPE_INT16 yytype_int16;
#else
typedef short int yytype_int16;
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif ! defined YYSIZE_T
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned int
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif

#ifndef YY_ATTRIBUTE
# if (defined __GNUC__                                               \
      && (2 < __GNUC__ || (__GNUC__ == 2 && 96 <= __GNUC_MINOR__)))  \
     || defined __SUNPRO_C && 0x5110 <= __SUNPRO_C
#  define YY_ATTRIBUTE(Spec) __attribute__(Spec)
# else
#  define YY_ATTRIBUTE(Spec) /* empty */
# endif
#endif

#ifndef YY_ATTRIBUTE_PURE
# define YY_ATTRIBUTE_PURE   YY_ATTRIBUTE ((__pure__))
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# define YY_ATTRIBUTE_UNUSED YY_ATTRIBUTE ((__unused__))
#endif

#if !defined _Noreturn \
     && (!defined __STDC_VERSION__ || __STDC_VERSION__ < 201112)
# if defined _MSC_VER && 1200 <= _MSC_VER
#  define _Noreturn __declspec (noreturn)
# else
#  define _Noreturn YY_ATTRIBUTE ((__noreturn__))
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(E) ((void) (E))
#else
# define YYUSE(E) /* empty */
#endif

#if defined __GNUC__ && 407 <= __GNUC__ * 100 + __GNUC_MINOR__
/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN \
    _Pragma ("GCC diagnostic push") \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")\
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# define YY_IGNORE_MAYBE_UNINITIALIZED_END \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif


#if ! defined yyoverflow || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */


#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yytype_int16 yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYSIZE_T yynewbytes;                                            \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / sizeof (*yyptr);                          \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, (Count) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYSIZE_T yyi;                         \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  3
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   339

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  75
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  76
/* YYNRULES -- Number of rules.  */
#define YYNRULES  193
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  285

/* YYTRANSLATE[YYX] -- Symbol number corresponding to YYX as returned
   by yylex, with out-of-bounds checking.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   329

#define YYTRANSLATE(YYX)                                                \
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, without out-of-bounds checking.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74
};

#if YYDEBUG
  /* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   139,   139,   142,   143,   146,   147,   148,   149,   150,
     151,   153,   159,   162,   168,   170,   171,   172,   172,   174,
     176,   179,   182,   186,   202,   203,   204,   204,   206,   206,
     208,   209,   209,   211,   211,   213,   213,   215,   217,   217,
     219,   221,   221,   223,   225,   225,   227,   229,   229,   231,
     231,   233,   233,   235,   235,   238,   238,   240,   240,   242,
     242,   244,   244,   246,   246,   248,   250,   250,   252,   268,
     276,   276,   278,   280,   280,   285,   294,   303,   314,   317,
     320,   321,   324,   325,   326,   327,   328,   337,   340,   341,
     344,   345,   346,   347,   348,   349,   350,   351,   352,   355,
     356,   359,   360,   361,   362,   363,   364,   365,   366,   367,
     368,   372,   375,   376,   379,   383,   386,   387,   390,   392,
     394,   396,   398,   400,   402,   404,   406,   408,   410,   412,
     414,   416,   418,   420,   422,   424,   426,   428,   430,   432,
     436,   440,   441,   444,   452,   452,   462,   472,   475,   476,
     479,   480,   483,   486,   487,   490,   495,   498,   498,   503,
     504,   504,   508,   509,   517,   520,   521,   524,   529,   537,
     540,   541,   544,   549,   552,   553,   556,   559,   562,   563,
     566,   572,   575,   576,   579,   584,   592,   593,   632,   633,
     634,   637,   649,   654
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || 0
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "LB", "RB", "LP", "RP", "MENUS", "MENU",
  "BUTTON", "DEFAULT_FUNCTION", "PLUS", "MINUS", "ALL", "OR", "CURSORS",
  "PIXMAPS", "ICONS", "COLOR", "SAVECOLOR", "MONOCHROME", "FUNCTION",
  "ICONMGR_SHOW", "ICONMGR", "WINDOW_FUNCTION", "ZOOM", "ICONMGRS",
  "ICONMGR_GEOMETRY", "ICONMGR_NOSHOW", "MAKE_TITLE", "GRAYSCALE",
  "ICONIFY_BY_UNMAPPING", "DONT_ICONIFY_BY_UNMAPPING", "NO_TITLE",
  "AUTO_RAISE", "NO_HILITE", "ICON_REGION", "META", "SHIFT", "LOCK",
  "CONTROL", "WINDOW", "TITLE", "ICON", "ROOT", "FRAME", "COLON", "EQUALS",
  "SQUEEZE_TITLE", "DONT_SQUEEZE_TITLE", "START_ICONIFIED",
  "NO_TITLE_HILITE", "TITLE_HILITE", "MOVE", "RESIZE", "WAIT", "SELECT",
  "KILL", "LEFT_TITLEBUTTON", "RIGHT_TITLEBUTTON", "NUMBER", "KEYWORD",
  "NKEYWORD", "CKEYWORD", "CLKEYWORD", "FKEYWORD", "FSKEYWORD", "SKEYWORD",
  "DKEYWORD", "JKEYWORD", "WINDOW_RING", "WARP_CURSOR", "ERRORTOKEN",
  "NO_STACKMODE", "STRING", "$accept", "twmrc", "stmts", "stmt", "$@1",
  "$@2", "$@3", "$@4", "$@5", "$@6", "$@7", "$@8", "$@9", "$@10", "$@11",
  "$@12", "$@13", "$@14", "$@15", "$@16", "$@17", "$@18", "$@19", "$@20",
  "$@21", "noarg", "sarg", "narg", "full", "fullkey", "keys", "key",
  "contexts", "context", "contextkeys", "contextkey", "pixmap_list",
  "pixmap_entries", "pixmap_entry", "cursor_list", "cursor_entries",
  "cursor_entry", "color_list", "color_entries", "color_entry", "$@22",
  "save_color_list", "s_color_entries", "s_color_entry", "win_color_list",
  "win_color_entries", "win_color_entry", "squeeze", "$@23", "$@24",
  "win_sqz_entries", "iconm_list", "iconm_entries", "iconm_entry",
  "win_list", "win_entries", "win_entry", "icon_list", "icon_entries",
  "icon_entry", "function", "function_entries", "function_entry", "menu",
  "menu_entries", "menu_entry", "action", "signed_number", "button",
  "string", "number", YY_NULLPTR
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[NUM] -- (External) token number corresponding to the
   (internal) symbol number NUM (which must be that of a token).  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   307,   308,   309,   310,   311,   312,   313,   314,
     315,   316,   317,   318,   319,   320,   321,   322,   323,   324,
     325,   326,   327,   328,   329
};
# endif

#define YYPACT_NINF -159

#define yypact_value_is_default(Yystate) \
  (!!((Yystate) == (-159)))

#define YYTABLE_NINF -161

#define yytable_value_is_error(Yytable_value) \
  0

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
static const yytype_int16 yypact[] =
{
    -159,    15,   265,  -159,  -159,   -56,   -30,   -25,    28,    30,
    -159,  -159,    32,  -159,   -56,  -159,   -25,   -30,  -159,   -56,
      40,  -159,  -159,    41,  -159,    42,  -159,    44,   -56,    46,
      48,  -159,    49,   -56,   -56,  -159,   -30,   -56,  -159,    58,
      60,  -159,  -159,  -159,  -159,  -159,  -159,   -28,    20,    61,
    -159,  -159,  -159,   -56,  -159,  -159,  -159,  -159,  -159,    65,
      66,  -159,  -159,    66,  -159,    76,  -159,  -159,    78,   -30,
      76,    76,    66,    76,    76,    76,    76,    76,    -4,    83,
      76,    76,    76,    43,    45,  -159,  -159,    76,    76,    76,
    -159,  -159,  -159,  -159,  -159,  -159,   -56,    85,  -159,   107,
       8,  -159,  -159,  -159,  -159,    -2,  -159,    86,  -159,  -159,
    -159,  -159,  -159,  -159,  -159,  -159,  -159,  -159,  -159,  -159,
    -159,    27,  -159,  -159,  -159,  -159,   -25,   -25,  -159,  -159,
    -159,    59,   153,    55,  -159,  -159,  -159,   -56,   -56,   -56,
     -56,   -56,   -56,   -56,   -56,   -56,   -56,   -56,  -159,  -159,
     -56,  -159,     0,     7,  -159,  -159,  -159,  -159,  -159,  -159,
       1,     2,   -30,     3,  -159,  -159,  -159,   -30,  -159,  -159,
    -159,  -159,  -159,  -159,   -56,     4,   -56,   -56,   -56,   -56,
     -56,   -56,   -56,   -56,   -56,   -56,   -56,  -159,  -159,  -159,
     -56,  -159,   -56,   -56,  -159,    17,  -159,  -159,  -159,  -159,
    -159,   -56,   -30,  -159,    33,  -159,    80,    13,    98,  -159,
    -159,    19,  -159,  -159,  -159,  -159,  -159,  -159,  -159,  -159,
    -159,  -159,  -159,  -159,  -159,   103,  -159,  -159,  -159,   -35,
    -159,     5,  -159,  -159,  -159,  -159,  -159,  -159,  -159,  -159,
    -159,   -25,  -159,  -159,  -159,  -159,  -159,  -159,  -159,  -159,
    -159,  -159,   -25,  -159,  -159,  -159,   -56,  -159,   104,   -30,
    -159,   -30,   -30,   -30,  -159,  -159,  -159,    85,    62,  -159,
    -159,  -159,  -159,  -159,  -159,  -159,   -56,     6,   106,  -159,
    -159,   -56,   -25,  -159,  -159
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       3,     0,     0,     1,     5,     0,     0,     0,     0,     0,
      59,    61,     0,    66,     0,    33,     0,    14,    31,     0,
      30,    47,    63,    19,    26,    46,    51,    40,     0,   156,
     159,    49,    37,     0,     0,    75,     0,     0,    73,    72,
      43,   192,     4,     6,     7,     8,     9,     0,     0,    55,
     193,   191,   186,     0,    68,   116,    16,   112,    15,     0,
       0,   148,    65,     0,    57,     0,    69,    13,     0,    12,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    77,    76,     0,     0,     0,
      80,    25,    23,    22,    80,    24,     0,     0,   187,     0,
       0,   174,    60,   141,    62,     0,    67,     0,   170,    34,
     165,    32,    11,    29,    48,    64,    18,    27,    45,    52,
      39,     0,   162,   161,    50,    36,     0,     0,    74,    71,
      42,     0,     0,     0,   182,    56,   115,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   117,   111,
       0,   113,     0,     0,   147,   151,   149,   150,   178,    58,
       0,     0,     0,     0,    20,    21,    87,    82,    83,    84,
      85,    88,    81,    99,     0,     0,   135,   127,   125,   121,
     123,   119,   129,   131,   133,   137,   139,   114,   173,   175,
       0,   140,     0,     0,   142,     0,   169,   171,   172,   164,
     166,     0,     0,   158,     0,    86,     0,     0,     0,   181,
     183,     0,   134,   126,   124,   120,   122,   118,   128,   130,
     132,   136,   138,   176,   146,   143,   177,   179,   180,     0,
      10,     0,    97,    98,    95,    96,    90,    91,    92,    93,
      94,     0,    89,   108,   109,   106,   107,   101,   102,   103,
     104,   105,     0,   100,   110,    53,     0,   184,     0,     0,
     167,     0,     0,     0,   188,    78,    79,     0,     0,   153,
     145,   168,   189,   190,   163,    54,     0,     0,     0,   152,
     154,     0,     0,   155,   185
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -159,  -159,  -159,  -159,  -159,  -159,  -159,  -159,  -159,  -159,
    -159,  -159,  -159,  -159,  -159,  -159,  -159,  -159,  -159,  -159,
    -159,  -159,  -159,  -159,  -159,  -159,  -159,  -159,  -159,  -159,
      16,  -159,  -159,  -159,  -159,  -159,  -159,  -159,  -159,  -159,
    -159,  -159,   -50,  -159,  -159,  -159,  -159,  -159,  -159,  -159,
    -159,  -159,  -159,  -159,  -159,  -159,  -159,  -159,  -159,   130,
    -159,  -159,  -159,  -159,  -159,  -159,  -159,  -159,  -158,  -159,
    -159,   -13,  -159,  -159,    -5,   -16
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     1,     2,    42,    73,    74,    70,    68,    65,    82,
      77,    89,    75,    71,    81,    76,   267,    97,   107,    59,
      60,    72,    63,    88,    87,    43,    44,    45,    91,    95,
     131,   172,   206,   242,   207,   253,    58,   100,   151,    56,
      99,   148,   104,   153,   194,   258,    62,   105,   156,   270,
     277,   280,    46,    79,    80,   163,   111,   161,   200,   109,
     160,   197,   102,   152,   189,   159,   195,   227,   135,   175,
     210,    54,   263,    47,    48,    51
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      49,    67,   154,    66,   188,   196,   199,   203,   209,    64,
     279,   191,   149,   106,    69,     3,   261,   262,    41,    90,
      85,   226,   115,    78,   256,    50,   243,   244,    83,    84,
      50,    55,    86,    57,    92,    61,   245,    52,    53,    41,
      52,    53,    93,   -28,   -17,   -44,    41,   -38,    98,  -157,
     246,  -160,   -35,   112,   247,   248,   249,   250,   251,   252,
     150,   -70,   155,   -41,   121,    50,    96,    94,   101,   103,
     192,   193,    41,   166,    41,    41,    41,    41,    41,   108,
      41,   110,    52,    53,    52,    53,   122,    41,   134,   158,
     126,   133,   127,   232,   233,   162,   167,   168,   169,   170,
     157,   174,   231,   234,   255,   171,  -144,   269,   276,   275,
     132,   136,   282,   164,   165,   137,   138,   235,     0,     0,
       0,   236,   237,   238,   239,   240,   241,     0,     0,     0,
     139,     0,   176,   177,   178,   179,   180,   181,   182,   183,
     184,   185,   186,     0,     0,   187,   202,   190,     0,   140,
     141,   205,   142,     0,     0,   198,   201,     0,   204,     0,
     143,   144,   145,   146,   147,     0,     0,   166,     0,   208,
     211,   212,   213,   214,   215,   216,   217,   218,   219,   220,
     221,   222,   228,     0,     0,   223,   230,   224,   225,     0,
     167,   168,   169,   170,     0,     0,   229,     0,   257,   173,
     113,   114,   254,   116,   117,   118,   119,   120,     0,     0,
     123,   124,   125,   260,     0,   264,     0,   128,   129,   130,
       0,     0,     0,     0,   259,     0,     0,     0,   265,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   266,
       0,     0,     0,   271,     0,   272,   273,   274,     0,     0,
       0,   268,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    -2,     4,     0,     0,   284,
       0,   278,   281,     5,     6,     7,   283,     0,     0,     0,
       8,     9,    10,    11,    12,    13,    14,    15,     0,    16,
      17,    18,    19,    20,    21,    22,    23,    24,    25,    26,
      27,    28,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    29,    30,    31,    32,     0,     0,     0,
       0,     0,     0,    33,    34,     0,    35,    36,     0,     0,
       0,     0,    37,     0,     0,    38,    39,     0,    40,    41
};

static const yytype_int16 yycheck[] =
{
       5,    17,     4,    16,     4,     4,     4,     4,     4,    14,
       4,     4,     4,    63,    19,     0,    11,    12,    74,    47,
      36,     4,    72,    28,     5,    60,    13,    14,    33,    34,
      60,     3,    37,     3,    47,     3,    23,    65,    66,    74,
      65,    66,    47,     3,     3,     3,    74,     3,    53,     3,
      37,     3,     3,    69,    41,    42,    43,    44,    45,    46,
      52,     3,    64,     3,    68,    60,     5,    47,     3,     3,
      63,    64,    74,    14,    74,    74,    74,    74,    74,     3,
      74,     3,    65,    66,    65,    66,     3,    74,     3,     3,
      47,    96,    47,    13,    14,    68,    37,    38,    39,    40,
     105,    46,    69,    23,     6,    46,     3,     3,    46,   267,
      94,     4,     6,   126,   127,     8,     9,    37,    -1,    -1,
      -1,    41,    42,    43,    44,    45,    46,    -1,    -1,    -1,
      23,    -1,   137,   138,   139,   140,   141,   142,   143,   144,
     145,   146,   147,    -1,    -1,   150,   162,   152,    -1,    42,
      43,   167,    45,    -1,    -1,   160,   161,    -1,   163,    -1,
      53,    54,    55,    56,    57,    -1,    -1,    14,    -1,   174,
     175,   176,   177,   178,   179,   180,   181,   182,   183,   184,
     185,   186,   195,    -1,    -1,   190,   202,   192,   193,    -1,
      37,    38,    39,    40,    -1,    -1,   201,    -1,   211,    46,
      70,    71,   207,    73,    74,    75,    76,    77,    -1,    -1,
      80,    81,    82,   229,    -1,   231,    -1,    87,    88,    89,
      -1,    -1,    -1,    -1,   229,    -1,    -1,    -1,   241,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   252,
      -1,    -1,    -1,   259,    -1,   261,   262,   263,    -1,    -1,
      -1,   256,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,     0,     1,    -1,    -1,   282,
      -1,   276,   277,     8,     9,    10,   281,    -1,    -1,    -1,
      15,    16,    17,    18,    19,    20,    21,    22,    -1,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    48,    49,    50,    51,    -1,    -1,    -1,
      -1,    -1,    -1,    58,    59,    -1,    61,    62,    -1,    -1,
      -1,    -1,    67,    -1,    -1,    70,    71,    -1,    73,    74
};

  /* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,    76,    77,     0,     1,     8,     9,    10,    15,    16,
      17,    18,    19,    20,    21,    22,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    35,    36,    48,
      49,    50,    51,    58,    59,    61,    62,    67,    70,    71,
      73,    74,    78,   100,   101,   102,   127,   148,   149,   149,
      60,   150,    65,    66,   146,     3,   114,     3,   111,    94,
      95,     3,   121,    97,   149,    83,   146,   150,    82,   149,
      81,    88,    96,    79,    80,    87,    90,    85,   149,   128,
     129,    89,    84,   149,   149,   150,   149,    99,    98,    86,
      47,   103,   146,   149,    47,   104,     5,    92,   149,   115,
     112,     3,   137,     3,   117,   122,   117,    93,     3,   134,
       3,   131,   150,   134,   134,   117,   134,   134,   134,   134,
     134,    68,     3,   134,   134,   134,    47,    47,   134,   134,
     134,   105,   105,   149,     3,   143,     4,     8,     9,    23,
      42,    43,    45,    53,    54,    55,    56,    57,   116,     4,
      52,   113,   138,   118,     4,    64,   123,   149,     3,   140,
     135,   132,    68,   130,   146,   146,    14,    37,    38,    39,
      40,    46,   106,    46,    46,   144,   149,   149,   149,   149,
     149,   149,   149,   149,   149,   149,   149,   149,     4,   139,
     149,     4,    63,    64,   119,   141,     4,   136,   149,     4,
     133,   149,   150,     4,   149,   150,   107,   109,   149,     4,
     145,   149,   149,   149,   149,   149,   149,   149,   149,   149,
     149,   149,   149,   149,   149,   149,     4,   142,   146,   149,
     150,    69,    13,    14,    23,    37,    41,    42,    43,    44,
      45,    46,   108,    13,    14,    23,    37,    41,    42,    43,
      44,    45,    46,   110,   149,     6,     5,   146,   120,   149,
     150,    11,    12,   147,   150,   146,   146,    91,   149,     3,
     124,   150,   150,   150,   150,   143,    46,   125,   149,     4,
     126,   149,     6,   149,   146
};

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    75,    76,    77,    77,    78,    78,    78,    78,    78,
      78,    78,    78,    78,    78,    78,    78,    79,    78,    78,
      78,    78,    78,    78,    78,    78,    80,    78,    81,    78,
      78,    82,    78,    83,    78,    84,    78,    78,    85,    78,
      78,    86,    78,    78,    87,    78,    78,    88,    78,    89,
      78,    90,    78,    91,    78,    92,    78,    93,    78,    94,
      78,    95,    78,    96,    78,    78,    97,    78,    78,    78,
      98,    78,    78,    99,    78,   100,   101,   102,   103,   104,
     105,   105,   106,   106,   106,   106,   106,   106,   107,   107,
     108,   108,   108,   108,   108,   108,   108,   108,   108,   109,
     109,   110,   110,   110,   110,   110,   110,   110,   110,   110,
     110,   111,   112,   112,   113,   114,   115,   115,   116,   116,
     116,   116,   116,   116,   116,   116,   116,   116,   116,   116,
     116,   116,   116,   116,   116,   116,   116,   116,   116,   116,
     117,   118,   118,   119,   120,   119,   119,   121,   122,   122,
     123,   123,   124,   125,   125,   126,   127,   128,   127,   127,
     129,   127,   130,   130,   131,   132,   132,   133,   133,   134,
     135,   135,   136,   137,   138,   138,   139,   140,   141,   141,
     142,   143,   144,   144,   145,   145,   146,   146,   147,   147,
     147,   148,   149,   150
};

  /* YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     1,     0,     2,     1,     1,     1,     1,     1,
       6,     3,     2,     2,     1,     2,     2,     0,     3,     1,
       4,     4,     2,     2,     2,     2,     0,     3,     0,     3,
       1,     0,     3,     0,     3,     0,     3,     1,     0,     3,
       1,     0,     3,     1,     0,     3,     1,     0,     3,     0,
       3,     0,     3,     0,     9,     0,     4,     0,     4,     0,
       3,     0,     3,     0,     3,     2,     0,     3,     2,     2,
       0,     3,     1,     0,     3,     1,     2,     2,     6,     6,
       0,     2,     1,     1,     1,     1,     2,     1,     0,     2,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     0,
       2,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     3,     0,     2,     2,     3,     0,     2,     3,     2,
       3,     2,     3,     2,     3,     2,     3,     2,     3,     2,
       3,     2,     3,     2,     3,     2,     3,     2,     3,     2,
       3,     0,     2,     2,     0,     4,     2,     3,     0,     2,
       1,     1,     3,     0,     2,     2,     1,     0,     5,     1,
       0,     3,     0,     5,     3,     0,     2,     3,     4,     3,
       0,     2,     1,     3,     0,     2,     2,     3,     0,     2,
       1,     3,     0,     2,     2,     7,     1,     2,     1,     2,
       2,     2,     1,     1
};


#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)
#define YYEMPTY         (-2)
#define YYEOF           0

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                  \
do                                                              \
  if (yychar == YYEMPTY)                                        \
    {                                                           \
      yychar = (Token);                                         \
      yylval = (Value);                                         \
      YYPOPSTACK (yylen);                                       \
      yystate = *yyssp;                                         \
      goto yybackup;                                            \
    }                                                           \
  else                                                          \
    {                                                           \
      yyerror (YY_("syntax error: cannot back up")); \
      YYERROR;                                                  \
    }                                                           \
while (0)

/* Error token number */
#define YYTERROR        1
#define YYERRCODE       256



/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)

/* This macro is provided for backward compatibility. */
#ifndef YY_LOCATION_PRINT
# define YY_LOCATION_PRINT(File, Loc) ((void) 0)
#endif


# define YY_SYMBOL_PRINT(Title, Type, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Type, Value); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*----------------------------------------.
| Print this symbol's value on YYOUTPUT.  |
`----------------------------------------*/

static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
{
  FILE *yyo = yyoutput;
  YYUSE (yyo);
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# endif
  YYUSE (yytype);
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
{
  YYFPRINTF (yyoutput, "%s %s (",
             yytype < YYNTOKENS ? "token" : "nterm", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yytype_int16 *yybottom, yytype_int16 *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yytype_int16 *yyssp, YYSTYPE *yyvsp, int yyrule)
{
  unsigned long int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       yystos[yyssp[yyi + 1 - yynrhs]],
                       &(yyvsp[(yyi + 1) - (yynrhs)])
                                              );
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, Rule); \
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif


#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined __GLIBC__ && defined _STRING_H
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
static YYSIZE_T
yystrlen (const char *yystr)
{
  YYSIZE_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
yystpcpy (char *yydest, const char *yysrc)
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

# ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYSIZE_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYSIZE_T yyn = 0;
      char const *yyp = yystr;

      for (;;)
        switch (*++yyp)
          {
          case '\'':
          case ',':
            goto do_not_strip_quotes;

          case '\\':
            if (*++yyp != '\\')
              goto do_not_strip_quotes;
            /* Fall through.  */
          default:
            if (yyres)
              yyres[yyn] = *yyp;
            yyn++;
            break;

          case '"':
            if (yyres)
              yyres[yyn] = '\0';
            return yyn;
          }
    do_not_strip_quotes: ;
    }

  if (! yyres)
    return yystrlen (yystr);

  return yystpcpy (yyres, yystr) - yyres;
}
# endif

/* Copy into *YYMSG, which is of size *YYMSG_ALLOC, an error message
   about the unexpected token YYTOKEN for the state stack whose top is
   YYSSP.

   Return 0 if *YYMSG was successfully written.  Return 1 if *YYMSG is
   not large enough to hold the message.  In that case, also set
   *YYMSG_ALLOC to the required number of bytes.  Return 2 if the
   required number of bytes is too large to store.  */
static int
yysyntax_error (YYSIZE_T *yymsg_alloc, char **yymsg,
                yytype_int16 *yyssp, int yytoken)
{
  YYSIZE_T yysize0 = yytnamerr (YY_NULLPTR, yytname[yytoken]);
  YYSIZE_T yysize = yysize0;
  enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
  /* Internationalized format string. */
  const char *yyformat = YY_NULLPTR;
  /* Arguments of yyformat. */
  char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
  /* Number of reported tokens (one for the "unexpected", one per
     "expected"). */
  int yycount = 0;

  /* There are many possibilities here to consider:
     - If this state is a consistent state with a default action, then
       the only way this function was invoked is if the default action
       is an error action.  In that case, don't check for expected
       tokens because there are none.
     - The only way there can be no lookahead present (in yychar) is if
       this state is a consistent state with a default action.  Thus,
       detecting the absence of a lookahead is sufficient to determine
       that there is no unexpected or expected token to report.  In that
       case, just report a simple "syntax error".
     - Don't assume there isn't a lookahead just because this state is a
       consistent state with a default action.  There might have been a
       previous inconsistent state, consistent state with a non-default
       action, or user semantic action that manipulated yychar.
     - Of course, the expected token list depends on states to have
       correct lookahead information, and it depends on the parser not
       to perform extra reductions after fetching a lookahead from the
       scanner and before detecting a syntax error.  Thus, state merging
       (from LALR or IELR) and default reductions corrupt the expected
       token list.  However, the list is correct for canonical LR with
       one exception: it will still contain any token that will not be
       accepted due to an error action in a later state.
  */
  if (yytoken != YYEMPTY)
    {
      int yyn = yypact[*yyssp];
      yyarg[yycount++] = yytname[yytoken];
      if (!yypact_value_is_default (yyn))
        {
          /* Start YYX at -YYN if negative to avoid negative indexes in
             YYCHECK.  In other words, skip the first -YYN actions for
             this state because they are default actions.  */
          int yyxbegin = yyn < 0 ? -yyn : 0;
          /* Stay within bounds of both yycheck and yytname.  */
          int yychecklim = YYLAST - yyn + 1;
          int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
          int yyx;

          for (yyx = yyxbegin; yyx < yyxend; ++yyx)
            if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR
                && !yytable_value_is_error (yytable[yyx + yyn]))
              {
                if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
                  {
                    yycount = 1;
                    yysize = yysize0;
                    break;
                  }
                yyarg[yycount++] = yytname[yyx];
                {
                  YYSIZE_T yysize1 = yysize + yytnamerr (YY_NULLPTR, yytname[yyx]);
                  if (! (yysize <= yysize1
                         && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
                    return 2;
                  yysize = yysize1;
                }
              }
        }
    }

  switch (yycount)
    {
# define YYCASE_(N, S)                      \
      case N:                               \
        yyformat = S;                       \
      break
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
# undef YYCASE_
    }

  {
    YYSIZE_T yysize1 = yysize + yystrlen (yyformat);
    if (! (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
      return 2;
    yysize = yysize1;
  }

  if (*yymsg_alloc < yysize)
    {
      *yymsg_alloc = 2 * yysize;
      if (! (yysize <= *yymsg_alloc
             && *yymsg_alloc <= YYSTACK_ALLOC_MAXIMUM))
        *yymsg_alloc = YYSTACK_ALLOC_MAXIMUM;
      return 1;
    }

  /* Avoid sprintf, as that infringes on the user's name space.
     Don't have undefined behavior even if the translation
     produced a string with the wrong number of "%s"s.  */
  {
    char *yyp = *yymsg;
    int yyi = 0;
    while ((*yyp = *yyformat) != '\0')
      if (*yyp == '%' && yyformat[1] == 's' && yyi < yycount)
        {
          yyp += yytnamerr (yyp, yyarg[yyi++]);
          yyformat += 2;
        }
      else
        {
          yyp++;
          yyformat++;
        }
  }
  return 0;
}
#endif /* YYERROR_VERBOSE */

/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep)
{
  YYUSE (yyvaluep);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YYUSE (yytype);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}




/* The lookahead symbol.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;
/* Number of syntax errors so far.  */
int yynerrs;


/*----------.
| yyparse.  |
`----------*/

int
yyparse (void)
{
    int yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       'yyss': related to states.
       'yyvs': related to semantic values.

       Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* The state stack.  */
    yytype_int16 yyssa[YYINITDEPTH];
    yytype_int16 *yyss;
    yytype_int16 *yyssp;

    /* The semantic value stack.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs;
    YYSTYPE *yyvsp;

    YYSIZE_T yystacksize;

  int yyn;
  int yyresult;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken = 0;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;

#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  yyssp = yyss = yyssa;
  yyvsp = yyvs = yyvsa;
  yystacksize = YYINITDEPTH;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY; /* Cause a token to be read.  */
  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        YYSTYPE *yyvs1 = yyvs;
        yytype_int16 *yyss1 = yyss;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * sizeof (*yyssp),
                    &yyvs1, yysize * sizeof (*yyvsp),
                    &yystacksize);

        yyss = yyss1;
        yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyexhaustedlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yytype_int16 *yyss1 = yyss;
        union yyalloc *yyptr =
          (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
        if (! yyptr)
          goto yyexhaustedlab;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
                  (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = yylex ();
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the shifted token.  */
  yychar = YYEMPTY;

  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- Do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     '$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 10:
#line 152 "gram.y" /* yacc.c:1646  */
    { AddIconRegion((yyvsp[-4].ptr), (yyvsp[-3].num), (yyvsp[-2].num), (yyvsp[-1].num), (yyvsp[0].num)); }
#line 1642 "gram.c" /* yacc.c:1646  */
    break;

  case 11:
#line 153 "gram.y" /* yacc.c:1646  */
    { if (Scr->FirstTime)
                                                  {
                                                    Scr->iconmgr.geometry=(yyvsp[-1].ptr);
                                                    Scr->iconmgr.columns=(yyvsp[0].num);
                                                  }
                                                }
#line 1653 "gram.c" /* yacc.c:1646  */
    break;

  case 12:
#line 159 "gram.y" /* yacc.c:1646  */
    { if (Scr->FirstTime)
                                                    Scr->iconmgr.geometry = (yyvsp[0].ptr);
                                                }
#line 1661 "gram.c" /* yacc.c:1646  */
    break;

  case 13:
#line 162 "gram.y" /* yacc.c:1646  */
    { if (Scr->FirstTime)
                                          {
                                                Scr->DoZoom = TRUE;
                                                Scr->ZoomCount = (short)(yyvsp[0].num);
                                          }
                                        }
#line 1672 "gram.c" /* yacc.c:1646  */
    break;

  case 14:
#line 168 "gram.y" /* yacc.c:1646  */
    { if (Scr->FirstTime)
                                                Scr->DoZoom = TRUE; }
#line 1679 "gram.c" /* yacc.c:1646  */
    break;

  case 15:
#line 170 "gram.y" /* yacc.c:1646  */
    {}
#line 1685 "gram.c" /* yacc.c:1646  */
    break;

  case 16:
#line 171 "gram.y" /* yacc.c:1646  */
    {}
#line 1691 "gram.c" /* yacc.c:1646  */
    break;

  case 17:
#line 172 "gram.y" /* yacc.c:1646  */
    { list = &Scr->IconifyByUn; }
#line 1697 "gram.c" /* yacc.c:1646  */
    break;

  case 19:
#line 174 "gram.y" /* yacc.c:1646  */
    { if (Scr->FirstTime)
                    Scr->IconifyByUnmapping = TRUE; }
#line 1704 "gram.c" /* yacc.c:1646  */
    break;

  case 20:
#line 176 "gram.y" /* yacc.c:1646  */
    {
                                          GotTitleButton ((yyvsp[-2].ptr), (yyvsp[0].num), False);
                                        }
#line 1712 "gram.c" /* yacc.c:1646  */
    break;

  case 21:
#line 179 "gram.y" /* yacc.c:1646  */
    {
                                          GotTitleButton ((yyvsp[-2].ptr), (yyvsp[0].num), True);
                                        }
#line 1720 "gram.c" /* yacc.c:1646  */
    break;

  case 22:
#line 182 "gram.y" /* yacc.c:1646  */
    { root = GetRoot((yyvsp[0].ptr), NULLSTR, NULLSTR);
                                          Scr->Mouse[(yyvsp[-1].num)][C_ROOT][0].func = F_MENU;
                                          Scr->Mouse[(yyvsp[-1].num)][C_ROOT][0].menu = root;
                                        }
#line 1729 "gram.c" /* yacc.c:1646  */
    break;

  case 23:
#line 186 "gram.y" /* yacc.c:1646  */
    { Scr->Mouse[(yyvsp[-1].num)][C_ROOT][0].func = (yyvsp[0].num);
                                          if ((yyvsp[0].num) == F_MENU)
                                          {
                                            pull->prev = NULL;
                                            Scr->Mouse[(yyvsp[-1].num)][C_ROOT][0].menu = pull;
                                          }
                                          else
                                          {
                                            root = GetRoot(TWM_ROOT,NULLSTR,NULLSTR);
                                            Scr->Mouse[(yyvsp[-1].num)][C_ROOT][0].item =
                                                AddToMenu(root,"x",Action,
                                                          NULL,(yyvsp[0].num),NULLSTR,NULLSTR);
                                          }
                                          Action = empty;
                                          pull = NULL;
                                        }
#line 1750 "gram.c" /* yacc.c:1646  */
    break;

  case 24:
#line 202 "gram.y" /* yacc.c:1646  */
    { GotKey((yyvsp[-1].ptr), (yyvsp[0].num)); }
#line 1756 "gram.c" /* yacc.c:1646  */
    break;

  case 25:
#line 203 "gram.y" /* yacc.c:1646  */
    { GotButton((yyvsp[-1].num), (yyvsp[0].num)); }
#line 1762 "gram.c" /* yacc.c:1646  */
    break;

  case 26:
#line 204 "gram.y" /* yacc.c:1646  */
    { list = &Scr->DontIconify; }
#line 1768 "gram.c" /* yacc.c:1646  */
    break;

  case 28:
#line 206 "gram.y" /* yacc.c:1646  */
    { list = &Scr->IconMgrNoShow; }
#line 1774 "gram.c" /* yacc.c:1646  */
    break;

  case 30:
#line 208 "gram.y" /* yacc.c:1646  */
    { Scr->IconManagerDontShow = TRUE; }
#line 1780 "gram.c" /* yacc.c:1646  */
    break;

  case 31:
#line 209 "gram.y" /* yacc.c:1646  */
    { list = &Scr->IconMgrs; }
#line 1786 "gram.c" /* yacc.c:1646  */
    break;

  case 33:
#line 211 "gram.y" /* yacc.c:1646  */
    { list = &Scr->IconMgrShow; }
#line 1792 "gram.c" /* yacc.c:1646  */
    break;

  case 35:
#line 213 "gram.y" /* yacc.c:1646  */
    { list = &Scr->NoTitleHighlight; }
#line 1798 "gram.c" /* yacc.c:1646  */
    break;

  case 37:
#line 215 "gram.y" /* yacc.c:1646  */
    { if (Scr->FirstTime)
                                                Scr->TitleHighlight = FALSE; }
#line 1805 "gram.c" /* yacc.c:1646  */
    break;

  case 38:
#line 217 "gram.y" /* yacc.c:1646  */
    { list = &Scr->NoHighlight; }
#line 1811 "gram.c" /* yacc.c:1646  */
    break;

  case 40:
#line 219 "gram.y" /* yacc.c:1646  */
    { if (Scr->FirstTime)
                                                Scr->Highlight = FALSE; }
#line 1818 "gram.c" /* yacc.c:1646  */
    break;

  case 41:
#line 221 "gram.y" /* yacc.c:1646  */
    { list = &Scr->NoStackModeL; }
#line 1824 "gram.c" /* yacc.c:1646  */
    break;

  case 43:
#line 223 "gram.y" /* yacc.c:1646  */
    { if (Scr->FirstTime)
                                                Scr->StackMode = FALSE; }
#line 1831 "gram.c" /* yacc.c:1646  */
    break;

  case 44:
#line 225 "gram.y" /* yacc.c:1646  */
    { list = &Scr->NoTitle; }
#line 1837 "gram.c" /* yacc.c:1646  */
    break;

  case 46:
#line 227 "gram.y" /* yacc.c:1646  */
    { if (Scr->FirstTime)
                                                Scr->NoTitlebar = TRUE; }
#line 1844 "gram.c" /* yacc.c:1646  */
    break;

  case 47:
#line 229 "gram.y" /* yacc.c:1646  */
    { list = &Scr->MakeTitle; }
#line 1850 "gram.c" /* yacc.c:1646  */
    break;

  case 49:
#line 231 "gram.y" /* yacc.c:1646  */
    { list = &Scr->StartIconified; }
#line 1856 "gram.c" /* yacc.c:1646  */
    break;

  case 51:
#line 233 "gram.y" /* yacc.c:1646  */
    { list = &Scr->AutoRaise; }
#line 1862 "gram.c" /* yacc.c:1646  */
    break;

  case 53:
#line 235 "gram.y" /* yacc.c:1646  */
    {
                                        root = GetRoot((yyvsp[-5].ptr), (yyvsp[-3].ptr), (yyvsp[-1].ptr)); }
#line 1869 "gram.c" /* yacc.c:1646  */
    break;

  case 54:
#line 237 "gram.y" /* yacc.c:1646  */
    { root->real_menu = TRUE;}
#line 1875 "gram.c" /* yacc.c:1646  */
    break;

  case 55:
#line 238 "gram.y" /* yacc.c:1646  */
    { root = GetRoot((yyvsp[0].ptr), NULLSTR, NULLSTR); }
#line 1881 "gram.c" /* yacc.c:1646  */
    break;

  case 56:
#line 239 "gram.y" /* yacc.c:1646  */
    { root->real_menu = TRUE; }
#line 1887 "gram.c" /* yacc.c:1646  */
    break;

  case 57:
#line 240 "gram.y" /* yacc.c:1646  */
    { root = GetRoot((yyvsp[0].ptr), NULLSTR, NULLSTR); }
#line 1893 "gram.c" /* yacc.c:1646  */
    break;

  case 59:
#line 242 "gram.y" /* yacc.c:1646  */
    { list = &Scr->IconNames; }
#line 1899 "gram.c" /* yacc.c:1646  */
    break;

  case 61:
#line 244 "gram.y" /* yacc.c:1646  */
    { color = COLOR; }
#line 1905 "gram.c" /* yacc.c:1646  */
    break;

  case 63:
#line 246 "gram.y" /* yacc.c:1646  */
    { color = GRAYSCALE; }
#line 1911 "gram.c" /* yacc.c:1646  */
    break;

  case 66:
#line 250 "gram.y" /* yacc.c:1646  */
    { color = MONOCHROME; }
#line 1917 "gram.c" /* yacc.c:1646  */
    break;

  case 68:
#line 252 "gram.y" /* yacc.c:1646  */
    { Scr->DefaultFunction.func = (yyvsp[0].num);
                                          if ((yyvsp[0].num) == F_MENU)
                                          {
                                            pull->prev = NULL;
                                            Scr->DefaultFunction.menu = pull;
                                          }
                                          else
                                          {
                                            root = GetRoot(TWM_ROOT,NULLSTR,NULLSTR);
                                            Scr->DefaultFunction.item =
                                                AddToMenu(root,"x",Action,
                                                          NULL,(yyvsp[0].num), NULLSTR, NULLSTR);
                                          }
                                          Action = empty;
                                          pull = NULL;
                                        }
#line 1938 "gram.c" /* yacc.c:1646  */
    break;

  case 69:
#line 268 "gram.y" /* yacc.c:1646  */
    { Scr->WindowFunction.func = (yyvsp[0].num);
                                           root = GetRoot(TWM_ROOT,NULLSTR,NULLSTR);
                                           Scr->WindowFunction.item =
                                                AddToMenu(root,"x",Action,
                                                          NULL,(yyvsp[0].num), NULLSTR, NULLSTR);
                                           Action = empty;
                                           pull = NULL;
                                        }
#line 1951 "gram.c" /* yacc.c:1646  */
    break;

  case 70:
#line 276 "gram.y" /* yacc.c:1646  */
    { list = &Scr->WarpCursorL; }
#line 1957 "gram.c" /* yacc.c:1646  */
    break;

  case 72:
#line 278 "gram.y" /* yacc.c:1646  */
    { if (Scr->FirstTime)
                                            Scr->WarpCursor = TRUE; }
#line 1964 "gram.c" /* yacc.c:1646  */
    break;

  case 73:
#line 280 "gram.y" /* yacc.c:1646  */
    { list = &Scr->WindowRingL; }
#line 1970 "gram.c" /* yacc.c:1646  */
    break;

  case 75:
#line 285 "gram.y" /* yacc.c:1646  */
    { if (!do_single_keyword ((yyvsp[0].num))) {
                                            parseWarning (
                                                 "unknown singleton keyword %d",
                                                 (yyvsp[0].num));
                                            ParseError = 1;
                                          }
                                        }
#line 1982 "gram.c" /* yacc.c:1646  */
    break;

  case 76:
#line 294 "gram.y" /* yacc.c:1646  */
    { if (!do_string_keyword ((yyvsp[-1].num), (yyvsp[0].ptr))) {
                                            parseWarning (
                                                 "unknown string keyword %d (value \"%s\")",
                                                 (yyvsp[-1].num), (yyvsp[0].ptr));
                                            ParseError = 1;
                                          }
                                        }
#line 1994 "gram.c" /* yacc.c:1646  */
    break;

  case 77:
#line 303 "gram.y" /* yacc.c:1646  */
    { if (!do_number_keyword ((yyvsp[-1].num), (yyvsp[0].num))) {
                                            parseWarning (
                                                 "unknown numeric keyword %d (value %d)",
                                                 (yyvsp[-1].num), (yyvsp[0].num));
                                            ParseError = 1;
                                          }
                                        }
#line 2006 "gram.c" /* yacc.c:1646  */
    break;

  case 78:
#line 314 "gram.y" /* yacc.c:1646  */
    { (yyval.num) = (yyvsp[0].num); }
#line 2012 "gram.c" /* yacc.c:1646  */
    break;

  case 79:
#line 317 "gram.y" /* yacc.c:1646  */
    { (yyval.num) = (yyvsp[0].num); }
#line 2018 "gram.c" /* yacc.c:1646  */
    break;

  case 82:
#line 324 "gram.y" /* yacc.c:1646  */
    { mods |= Mod1Mask; }
#line 2024 "gram.c" /* yacc.c:1646  */
    break;

  case 83:
#line 325 "gram.y" /* yacc.c:1646  */
    { mods |= ShiftMask; }
#line 2030 "gram.c" /* yacc.c:1646  */
    break;

  case 84:
#line 326 "gram.y" /* yacc.c:1646  */
    { mods |= LockMask; }
#line 2036 "gram.c" /* yacc.c:1646  */
    break;

  case 85:
#line 327 "gram.y" /* yacc.c:1646  */
    { mods |= ControlMask; }
#line 2042 "gram.c" /* yacc.c:1646  */
    break;

  case 86:
#line 328 "gram.y" /* yacc.c:1646  */
    { if ((yyvsp[0].num) < 1 || (yyvsp[0].num) > 5) {
                                             parseWarning (
                                                  "bad modifier number (%d), must be 1-5",
                                                  (yyvsp[0].num));
                                             ParseError = 1;
                                          } else {
                                             mods |= (Mod1Mask << ((yyvsp[0].num) - 1));
                                          }
                                        }
#line 2056 "gram.c" /* yacc.c:1646  */
    break;

  case 87:
#line 337 "gram.y" /* yacc.c:1646  */
    { }
#line 2062 "gram.c" /* yacc.c:1646  */
    break;

  case 90:
#line 344 "gram.y" /* yacc.c:1646  */
    { cont |= C_WINDOW_BIT; }
#line 2068 "gram.c" /* yacc.c:1646  */
    break;

  case 91:
#line 345 "gram.y" /* yacc.c:1646  */
    { cont |= C_TITLE_BIT; }
#line 2074 "gram.c" /* yacc.c:1646  */
    break;

  case 92:
#line 346 "gram.y" /* yacc.c:1646  */
    { cont |= C_ICON_BIT; }
#line 2080 "gram.c" /* yacc.c:1646  */
    break;

  case 93:
#line 347 "gram.y" /* yacc.c:1646  */
    { cont |= C_ROOT_BIT; }
#line 2086 "gram.c" /* yacc.c:1646  */
    break;

  case 94:
#line 348 "gram.y" /* yacc.c:1646  */
    { cont |= C_FRAME_BIT; }
#line 2092 "gram.c" /* yacc.c:1646  */
    break;

  case 95:
#line 349 "gram.y" /* yacc.c:1646  */
    { cont |= C_ICONMGR_BIT; }
#line 2098 "gram.c" /* yacc.c:1646  */
    break;

  case 96:
#line 350 "gram.y" /* yacc.c:1646  */
    { cont |= C_ICONMGR_BIT; }
#line 2104 "gram.c" /* yacc.c:1646  */
    break;

  case 97:
#line 351 "gram.y" /* yacc.c:1646  */
    { cont |= C_ALL_BITS; }
#line 2110 "gram.c" /* yacc.c:1646  */
    break;

  case 98:
#line 352 "gram.y" /* yacc.c:1646  */
    {  }
#line 2116 "gram.c" /* yacc.c:1646  */
    break;

  case 101:
#line 359 "gram.y" /* yacc.c:1646  */
    { cont |= C_WINDOW_BIT; }
#line 2122 "gram.c" /* yacc.c:1646  */
    break;

  case 102:
#line 360 "gram.y" /* yacc.c:1646  */
    { cont |= C_TITLE_BIT; }
#line 2128 "gram.c" /* yacc.c:1646  */
    break;

  case 103:
#line 361 "gram.y" /* yacc.c:1646  */
    { cont |= C_ICON_BIT; }
#line 2134 "gram.c" /* yacc.c:1646  */
    break;

  case 104:
#line 362 "gram.y" /* yacc.c:1646  */
    { cont |= C_ROOT_BIT; }
#line 2140 "gram.c" /* yacc.c:1646  */
    break;

  case 105:
#line 363 "gram.y" /* yacc.c:1646  */
    { cont |= C_FRAME_BIT; }
#line 2146 "gram.c" /* yacc.c:1646  */
    break;

  case 106:
#line 364 "gram.y" /* yacc.c:1646  */
    { cont |= C_ICONMGR_BIT; }
#line 2152 "gram.c" /* yacc.c:1646  */
    break;

  case 107:
#line 365 "gram.y" /* yacc.c:1646  */
    { cont |= C_ICONMGR_BIT; }
#line 2158 "gram.c" /* yacc.c:1646  */
    break;

  case 108:
#line 366 "gram.y" /* yacc.c:1646  */
    { cont |= C_ALL_BITS; }
#line 2164 "gram.c" /* yacc.c:1646  */
    break;

  case 109:
#line 367 "gram.y" /* yacc.c:1646  */
    { }
#line 2170 "gram.c" /* yacc.c:1646  */
    break;

  case 110:
#line 368 "gram.y" /* yacc.c:1646  */
    { Name = (yyvsp[0].ptr); cont |= C_NAME_BIT; }
#line 2176 "gram.c" /* yacc.c:1646  */
    break;

  case 114:
#line 379 "gram.y" /* yacc.c:1646  */
    { SetHighlightPixmap ((yyvsp[0].ptr)); }
#line 2182 "gram.c" /* yacc.c:1646  */
    break;

  case 118:
#line 390 "gram.y" /* yacc.c:1646  */
    {
                        NewBitmapCursor(&Scr->FrameCursor, (yyvsp[-1].ptr), (yyvsp[0].ptr)); }
#line 2189 "gram.c" /* yacc.c:1646  */
    break;

  case 119:
#line 392 "gram.y" /* yacc.c:1646  */
    {
                        NewFontCursor(&Scr->FrameCursor, (yyvsp[0].ptr)); }
#line 2196 "gram.c" /* yacc.c:1646  */
    break;

  case 120:
#line 394 "gram.y" /* yacc.c:1646  */
    {
                        NewBitmapCursor(&Scr->TitleCursor, (yyvsp[-1].ptr), (yyvsp[0].ptr)); }
#line 2203 "gram.c" /* yacc.c:1646  */
    break;

  case 121:
#line 396 "gram.y" /* yacc.c:1646  */
    {
                        NewFontCursor(&Scr->TitleCursor, (yyvsp[0].ptr)); }
#line 2210 "gram.c" /* yacc.c:1646  */
    break;

  case 122:
#line 398 "gram.y" /* yacc.c:1646  */
    {
                        NewBitmapCursor(&Scr->IconCursor, (yyvsp[-1].ptr), (yyvsp[0].ptr)); }
#line 2217 "gram.c" /* yacc.c:1646  */
    break;

  case 123:
#line 400 "gram.y" /* yacc.c:1646  */
    {
                        NewFontCursor(&Scr->IconCursor, (yyvsp[0].ptr)); }
#line 2224 "gram.c" /* yacc.c:1646  */
    break;

  case 124:
#line 402 "gram.y" /* yacc.c:1646  */
    {
                        NewBitmapCursor(&Scr->IconMgrCursor, (yyvsp[-1].ptr), (yyvsp[0].ptr)); }
#line 2231 "gram.c" /* yacc.c:1646  */
    break;

  case 125:
#line 404 "gram.y" /* yacc.c:1646  */
    {
                        NewFontCursor(&Scr->IconMgrCursor, (yyvsp[0].ptr)); }
#line 2238 "gram.c" /* yacc.c:1646  */
    break;

  case 126:
#line 406 "gram.y" /* yacc.c:1646  */
    {
                        NewBitmapCursor(&Scr->ButtonCursor, (yyvsp[-1].ptr), (yyvsp[0].ptr)); }
#line 2245 "gram.c" /* yacc.c:1646  */
    break;

  case 127:
#line 408 "gram.y" /* yacc.c:1646  */
    {
                        NewFontCursor(&Scr->ButtonCursor, (yyvsp[0].ptr)); }
#line 2252 "gram.c" /* yacc.c:1646  */
    break;

  case 128:
#line 410 "gram.y" /* yacc.c:1646  */
    {
                        NewBitmapCursor(&Scr->MoveCursor, (yyvsp[-1].ptr), (yyvsp[0].ptr)); }
#line 2259 "gram.c" /* yacc.c:1646  */
    break;

  case 129:
#line 412 "gram.y" /* yacc.c:1646  */
    {
                        NewFontCursor(&Scr->MoveCursor, (yyvsp[0].ptr)); }
#line 2266 "gram.c" /* yacc.c:1646  */
    break;

  case 130:
#line 414 "gram.y" /* yacc.c:1646  */
    {
                        NewBitmapCursor(&Scr->ResizeCursor, (yyvsp[-1].ptr), (yyvsp[0].ptr)); }
#line 2273 "gram.c" /* yacc.c:1646  */
    break;

  case 131:
#line 416 "gram.y" /* yacc.c:1646  */
    {
                        NewFontCursor(&Scr->ResizeCursor, (yyvsp[0].ptr)); }
#line 2280 "gram.c" /* yacc.c:1646  */
    break;

  case 132:
#line 418 "gram.y" /* yacc.c:1646  */
    {
                        NewBitmapCursor(&Scr->WaitCursor, (yyvsp[-1].ptr), (yyvsp[0].ptr)); }
#line 2287 "gram.c" /* yacc.c:1646  */
    break;

  case 133:
#line 420 "gram.y" /* yacc.c:1646  */
    {
                        NewFontCursor(&Scr->WaitCursor, (yyvsp[0].ptr)); }
#line 2294 "gram.c" /* yacc.c:1646  */
    break;

  case 134:
#line 422 "gram.y" /* yacc.c:1646  */
    {
                        NewBitmapCursor(&Scr->MenuCursor, (yyvsp[-1].ptr), (yyvsp[0].ptr)); }
#line 2301 "gram.c" /* yacc.c:1646  */
    break;

  case 135:
#line 424 "gram.y" /* yacc.c:1646  */
    {
                        NewFontCursor(&Scr->MenuCursor, (yyvsp[0].ptr)); }
#line 2308 "gram.c" /* yacc.c:1646  */
    break;

  case 136:
#line 426 "gram.y" /* yacc.c:1646  */
    {
                        NewBitmapCursor(&Scr->SelectCursor, (yyvsp[-1].ptr), (yyvsp[0].ptr)); }
#line 2315 "gram.c" /* yacc.c:1646  */
    break;

  case 137:
#line 428 "gram.y" /* yacc.c:1646  */
    {
                        NewFontCursor(&Scr->SelectCursor, (yyvsp[0].ptr)); }
#line 2322 "gram.c" /* yacc.c:1646  */
    break;

  case 138:
#line 430 "gram.y" /* yacc.c:1646  */
    {
                        NewBitmapCursor(&Scr->DestroyCursor, (yyvsp[-1].ptr), (yyvsp[0].ptr)); }
#line 2329 "gram.c" /* yacc.c:1646  */
    break;

  case 139:
#line 432 "gram.y" /* yacc.c:1646  */
    {
                        NewFontCursor(&Scr->DestroyCursor, (yyvsp[0].ptr)); }
#line 2336 "gram.c" /* yacc.c:1646  */
    break;

  case 143:
#line 444 "gram.y" /* yacc.c:1646  */
    { if (!do_colorlist_keyword ((yyvsp[-1].num), color,
                                                                     (yyvsp[0].ptr))) {
                                            parseWarning (
                                                 "unhandled list color keyword %d (string \"%s\")",
                                                 (yyvsp[-1].num), (yyvsp[0].ptr));
                                            ParseError = 1;
                                          }
                                        }
#line 2349 "gram.c" /* yacc.c:1646  */
    break;

  case 144:
#line 452 "gram.y" /* yacc.c:1646  */
    { list = do_colorlist_keyword((yyvsp[-1].num),color,
                                                                      (yyvsp[0].ptr));
                                          if (!list) {
                                            parseWarning (
                                                 "unhandled color list keyword %d (string \"%s\")",
                                                 (yyvsp[-1].num), (yyvsp[0].ptr));
                                            ParseError = 1;
                                          }
                                        }
#line 2363 "gram.c" /* yacc.c:1646  */
    break;

  case 145:
#line 461 "gram.y" /* yacc.c:1646  */
    { /* No action */; }
#line 2369 "gram.c" /* yacc.c:1646  */
    break;

  case 146:
#line 462 "gram.y" /* yacc.c:1646  */
    { if (!do_color_keyword ((yyvsp[-1].num), color,
                                                                 (yyvsp[0].ptr))) {
                                            parseWarning (
                                                 "unhandled color keyword %d (string \"%s\")",
                                                 (yyvsp[-1].num), (yyvsp[0].ptr));
                                            ParseError = 1;
                                          }
                                        }
#line 2382 "gram.c" /* yacc.c:1646  */
    break;

  case 150:
#line 479 "gram.y" /* yacc.c:1646  */
    { do_string_savecolor(color, (yyvsp[0].ptr)); }
#line 2388 "gram.c" /* yacc.c:1646  */
    break;

  case 151:
#line 480 "gram.y" /* yacc.c:1646  */
    { do_var_savecolor((yyvsp[0].num)); }
#line 2394 "gram.c" /* yacc.c:1646  */
    break;

  case 155:
#line 490 "gram.y" /* yacc.c:1646  */
    { if (Scr->FirstTime &&
                                              color == Scr->Monochrome)
                                            AddToList(list, (yyvsp[-1].ptr), (yyvsp[0].ptr)); }
#line 2402 "gram.c" /* yacc.c:1646  */
    break;

  case 156:
#line 495 "gram.y" /* yacc.c:1646  */
    {
                                    if (HasShape) Scr->SqueezeTitle = TRUE;
                                }
#line 2410 "gram.c" /* yacc.c:1646  */
    break;

  case 157:
#line 498 "gram.y" /* yacc.c:1646  */
    { list = &Scr->SqueezeTitleL;
                                  if (HasShape && Scr->SqueezeTitle == -1)
                                    Scr->SqueezeTitle = TRUE;
                                }
#line 2419 "gram.c" /* yacc.c:1646  */
    break;

  case 159:
#line 503 "gram.y" /* yacc.c:1646  */
    { Scr->SqueezeTitle = FALSE; }
#line 2425 "gram.c" /* yacc.c:1646  */
    break;

  case 160:
#line 504 "gram.y" /* yacc.c:1646  */
    { list = &Scr->DontSqueezeTitleL; }
#line 2431 "gram.c" /* yacc.c:1646  */
    break;

  case 163:
#line 509 "gram.y" /* yacc.c:1646  */
    {
                                if (Scr->FirstTime) {
                                   do_squeeze_entry (list, (yyvsp[-3].ptr), (yyvsp[-2].num), (yyvsp[-1].num), (yyvsp[0].num));
                                }
                        }
#line 2441 "gram.c" /* yacc.c:1646  */
    break;

  case 167:
#line 524 "gram.y" /* yacc.c:1646  */
    { if (Scr->FirstTime)
                                            AddToList(list, (yyvsp[-2].ptr), (char *)
                                                AllocateIconManager((yyvsp[-2].ptr), NULLSTR,
                                                        (yyvsp[-1].ptr),(yyvsp[0].num)));
                                        }
#line 2451 "gram.c" /* yacc.c:1646  */
    break;

  case 168:
#line 530 "gram.y" /* yacc.c:1646  */
    { if (Scr->FirstTime)
                                            AddToList(list, (yyvsp[-3].ptr), (char *)
                                                AllocateIconManager((yyvsp[-3].ptr),(yyvsp[-2].ptr),
                                                (yyvsp[-1].ptr), (yyvsp[0].num)));
                                        }
#line 2461 "gram.c" /* yacc.c:1646  */
    break;

  case 172:
#line 544 "gram.y" /* yacc.c:1646  */
    { if (Scr->FirstTime)
                                            AddToList(list, (yyvsp[0].ptr), 0);
                                        }
#line 2469 "gram.c" /* yacc.c:1646  */
    break;

  case 176:
#line 556 "gram.y" /* yacc.c:1646  */
    { if (Scr->FirstTime) AddToList(list, (yyvsp[-1].ptr), (yyvsp[0].ptr)); }
#line 2475 "gram.c" /* yacc.c:1646  */
    break;

  case 180:
#line 566 "gram.y" /* yacc.c:1646  */
    { AddToMenu(root, empty, Action, NULL, (yyvsp[0].num),
                                                NULLSTR, NULLSTR);
                                          Action = empty;
                                        }
#line 2484 "gram.c" /* yacc.c:1646  */
    break;

  case 184:
#line 579 "gram.y" /* yacc.c:1646  */
    { AddToMenu(root, (yyvsp[-1].ptr), Action, pull, (yyvsp[0].num),
                                                NULLSTR, NULLSTR);
                                          Action = empty;
                                          pull = NULL;
                                        }
#line 2494 "gram.c" /* yacc.c:1646  */
    break;

  case 185:
#line 584 "gram.y" /* yacc.c:1646  */
    {
                                          AddToMenu(root, (yyvsp[-6].ptr), Action, pull, (yyvsp[0].num),
                                                (yyvsp[-4].ptr), (yyvsp[-2].ptr));
                                          Action = empty;
                                          pull = NULL;
                                        }
#line 2505 "gram.c" /* yacc.c:1646  */
    break;

  case 186:
#line 592 "gram.y" /* yacc.c:1646  */
    { (yyval.num) = (yyvsp[0].num); }
#line 2511 "gram.c" /* yacc.c:1646  */
    break;

  case 187:
#line 593 "gram.y" /* yacc.c:1646  */
    {
                                (yyval.num) = (yyvsp[-1].num);
                                Action = (yyvsp[0].ptr);
                                switch ((yyvsp[-1].num)) {
                                  case F_MENU:
                                    pull = GetRoot ((yyvsp[0].ptr), NULLSTR,NULLSTR);
                                    pull->prev = root;
                                    break;
                                  case F_WARPRING:
                                    if (!CheckWarpRingArg (Action)) {
                                        parseWarning (
                                             "ignoring invalid f.warptoring argument \"%s\"",
                                             Action);
                                        (yyval.num) = F_NOP;
                                    }
                                    break;
                                  case F_WARPTOSCREEN:
                                    if (!CheckWarpScreenArg (Action)) {
                                        parseWarning (
                                             "ignoring invalid f.warptoscreen argument \"%s\"",
                                             Action);
                                        (yyval.num) = F_NOP;
                                    }
                                    break;
                                  case F_COLORMAP:
                                    if (CheckColormapArg (Action)) {
                                        (yyval.num) = F_COLORMAP;
                                    } else {
                                        parseWarning (
                                             "ignoring invalid f.colormap argument \"%s\"",
                                             Action);
                                        (yyval.num) = F_NOP;
                                    }
                                    break;
                                } /* end switch */
                                   }
#line 2552 "gram.c" /* yacc.c:1646  */
    break;

  case 188:
#line 632 "gram.y" /* yacc.c:1646  */
    { (yyval.num) = (yyvsp[0].num); }
#line 2558 "gram.c" /* yacc.c:1646  */
    break;

  case 189:
#line 633 "gram.y" /* yacc.c:1646  */
    { (yyval.num) = (yyvsp[0].num); }
#line 2564 "gram.c" /* yacc.c:1646  */
    break;

  case 190:
#line 634 "gram.y" /* yacc.c:1646  */
    { (yyval.num) = -((yyvsp[0].num)); }
#line 2570 "gram.c" /* yacc.c:1646  */
    break;

  case 191:
#line 637 "gram.y" /* yacc.c:1646  */
    { (yyval.num) = (yyvsp[0].num);
                                          if ((yyvsp[0].num) == 0)
                                                yyerror("bad button 0");

                                          if ((yyvsp[0].num) > MAX_BUTTONS)
                                          {
                                                (yyval.num) = 0;
                                                yyerror("button number too large");
                                          }
                                        }
#line 2585 "gram.c" /* yacc.c:1646  */
    break;

  case 192:
#line 649 "gram.y" /* yacc.c:1646  */
    { ptr = strdup((yyvsp[0].ptr));
                                          RemoveDQuote(ptr);
                                          (yyval.ptr) = ptr;
                                        }
#line 2594 "gram.c" /* yacc.c:1646  */
    break;

  case 193:
#line 654 "gram.y" /* yacc.c:1646  */
    { (yyval.num) = (yyvsp[0].num); }
#line 2600 "gram.c" /* yacc.c:1646  */
    break;


#line 2604 "gram.c" /* yacc.c:1646  */
      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYEMPTY : YYTRANSLATE (yychar);

  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (YY_("syntax error"));
#else
# define YYSYNTAX_ERROR yysyntax_error (&yymsg_alloc, &yymsg, \
                                        yyssp, yytoken)
      {
        char const *yymsgp = YY_("syntax error");
        int yysyntax_error_status;
        yysyntax_error_status = YYSYNTAX_ERROR;
        if (yysyntax_error_status == 0)
          yymsgp = yymsg;
        else if (yysyntax_error_status == 1)
          {
            if (yymsg != yymsgbuf)
              YYSTACK_FREE (yymsg);
            yymsg = (char *) YYSTACK_ALLOC (yymsg_alloc);
            if (!yymsg)
              {
                yymsg = yymsgbuf;
                yymsg_alloc = sizeof yymsgbuf;
                yysyntax_error_status = 2;
              }
            else
              {
                yysyntax_error_status = YYSYNTAX_ERROR;
                yymsgp = yymsg;
              }
          }
        yyerror (yymsgp);
        if (yysyntax_error_status == 2)
          goto yyexhaustedlab;
      }
# undef YYSYNTAX_ERROR
#endif
    }



  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

      if (yychar <= YYEOF)
        {
          /* Return failure if at end of input.  */
          if (yychar == YYEOF)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval);
          yychar = YYEMPTY;
        }
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:

  /* Pacify compilers like GCC when the user code never invokes
     YYERROR and the label yyerrorlab therefore never appears in user
     code.  */
  if (/*CONSTCOND*/ 0)
     goto yyerrorlab;

  /* Do not reclaim the symbols of the rule whose action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYTERROR;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
            {
              yyn = yytable[yyn];
              if (0 < yyn)
                break;
            }
        }

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
        YYABORT;


      yydestruct ("Error: popping",
                  yystos[yystate], yyvsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;

#if !defined yyoverflow || YYERROR_VERBOSE
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  yystos[*yyssp], yyvsp);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
#if YYERROR_VERBOSE
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
#endif
  return yyresult;
}
#line 657 "gram.y" /* yacc.c:1906  */


static void
yyerror(const char *s)
{
    parseWarning("error in input file:  %s", s ? s : "");
    ParseError = 1;
}

static void
RemoveDQuote(char *str)
{
    register char *i, *o;
    register int n;
    register int count;

    for (i = str + 1, o = str; *i && *i != '\"'; o++) {
        if (*i == '\\') {
            switch (*++i) {
            case 'n':
                *o = '\n';
                i++;
                break;
            case 'b':
                *o = '\b';
                i++;
                break;
            case 'r':
                *o = '\r';
                i++;
                break;
            case 't':
                *o = '\t';
                i++;
                break;
            case 'f':
                *o = '\f';
                i++;
                break;
            case '0':
                if (*++i == 'x')
                    goto hex;
                else
                    --i;
                /* FALLTHRU */
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
                n = 0;
                count = 0;
                while (*i >= '0' && *i <= '7' && count < 3) {
                    n = (n << 3) + (*i++ - '0');
                    count++;
                }
                *o = (char) n;
                break;
              hex:
            case 'x':
                n = 0;
                count = 0;
                while (i++, count++ < 2) {
                    if (*i >= '0' && *i <= '9')
                        n = (n << 4) + (*i - '0');
                    else if (*i >= 'a' && *i <= 'f')
                        n = (n << 4) + (*i - 'a') + 10;
                    else if (*i >= 'A' && *i <= 'F')
                        n = (n << 4) + (*i - 'A') + 10;
                    else
                        break;
                }
                *o = (char) n;
                break;
            case '\n':
                i++;            /* punt */
                o--;            /* to account for o++ at end of loop */
                break;
            case '\"':
            case '\'':
            case '\\':
            default:
                *o = *i++;
                break;
            }
        }
        else
            *o = *i++;
    }
    *o = '\0';
}

static MenuRoot *
GetRoot(const char *name, const char *fore, const char *back)
{
    MenuRoot *tmp;

    tmp = FindMenuRoot(name);
    if (tmp == NULL)
        tmp = NewMenuRoot(name);

    if (fore) {
        int save;

        save = Scr->FirstTime;
        Scr->FirstTime = TRUE;
        GetColor(COLOR, &tmp->hi_fore, fore);
        GetColor(COLOR, &tmp->hi_back, back);
        Scr->FirstTime = (short) save;
    }

    return tmp;
}

static void
GotButton(int butt, int func)
{
    int i;

    for (i = 0; i < NUM_CONTEXTS; i++) {
        if ((cont & (1 << i)) == 0)
            continue;

        Scr->Mouse[butt][i][mods].func = func;
        if (func == F_MENU) {
            pull->prev = NULL;
            Scr->Mouse[butt][i][mods].menu = pull;
        }
        else {
            root = GetRoot(TWM_ROOT, NULLSTR, NULLSTR);
            Scr->Mouse[butt][i][mods].item = AddToMenu(root, "x", Action,
                                                       NULL, func, NULLSTR, NULLSTR);
        }
    }
    Action = empty;
    pull = NULL;
    cont = 0;
    mods_used |= (unsigned) mods;
    mods = 0;
}

static void
GotKey(char *key, int func)
{
    int i;

    for (i = 0; i < NUM_CONTEXTS; i++) {
        if ((cont & (1 << i)) == 0)
            continue;
        if (!AddFuncKey(key, i, mods, func, Name, Action))
            break;
    }

    Action = empty;
    pull = NULL;
    cont = 0;
    mods_used |= (unsigned) mods;
    mods = 0;
}

static void
GotTitleButton(char *bitmapname, int func, Bool rightside)
{
    if (!CreateTitleButton(bitmapname, func, Action, pull, rightside, True)) {
        parseWarning("unable to create %s titlebutton \"%s\"",
                     rightside ? "right" : "left", bitmapname);
    }
    Action = empty;
    pull = NULL;
}

static Bool
CheckWarpScreenArg(char *s)
{
    XmuCopyISOLatin1Lowered(s, s);

    if (strcmp(s, WARPSCREEN_NEXT) == 0 ||
        strcmp(s, WARPSCREEN_PREV) == 0 ||
        strcmp(s, WARPSCREEN_BACK) == 0)
        return True;

    for (; *s && isascii(*s) && isdigit(*s); s++);      /* SUPPRESS 530 */
    return (*s ? False : True);
}

static Bool
CheckWarpRingArg(char *s)
{
    XmuCopyISOLatin1Lowered(s, s);

    if (strcmp(s, WARPSCREEN_NEXT) == 0 ||
        strcmp(s, WARPSCREEN_PREV) == 0)
        return True;

    return False;
}

static Bool
CheckColormapArg(char *s)
{
    XmuCopyISOLatin1Lowered(s, s);

    if (strcmp(s, COLORMAP_NEXT) == 0 ||
        strcmp(s, COLORMAP_PREV) == 0 ||
        strcmp(s, COLORMAP_DEFAULT) == 0)
        return True;

    return False;
}
