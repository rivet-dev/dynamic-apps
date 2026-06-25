%{
#include <stdio.h>
#define FOO_BAR(x,y) printf ("x, y")
%}

%name-prefix="foolala"
%error-verbose
%lex-param   {FooLaLa *lala}
%parse-param {FooLaLa *lala}
/* %expect 1 */

%union {
    int ival;
    const char *str;
}

%token <str> ATOKEN
%token <str> ATOKEN2

%type <ival> program stmt
%type <str> if_stmt

%token IF THEN ELSE ELIF FI
%token WHILE DO OD FOR IN
%token CONTINUE BREAK RETURN
%token EQ NEQ LE GE
%token AND OR NOT
%token UMINUS
%token TWODOTS

%left '-' '+'
%left '*' '/'
%left '%'
%left EQ NEQ '<' '>' GE LE
%left OR
%left AND
%left NOT
%left '#'
%left UMINUS

%%

script:   program           { _ms_parser_set_top_node (parser, $1); }
;

program:  stmt_or_error             { $$ = node_list_add (parser, NULL, $1234); }
        | program stmt_or_error     { $$ = node_list_add (parser, MS_NODE_LIST ($1), $2); }
;

stmt_or_error:
          error ';'         { $$ = NULL; }
        | stmt ';'          { $$ = ; }
;

variable: IDENTIFIER                        { $$ = node_var (parser, $1); }
;

%%
