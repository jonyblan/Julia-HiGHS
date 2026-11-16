
%{
#include "../../support/type/TokenLabel.h"
#include "AbstractSyntaxTree.h"
#include "BisonActions.h"
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>

typedef struct YYLTYPE YYLTYPE;
void yyerror(const YYLTYPE *location, const char *message);

static int parseDimension(const char *text) {
    char *end;
    errno = 0;
    long value = strtol(text, &end, 10);
    return errno || *end || value < 1 || value > INT_MAX ? 0 : (int)value;
}
%}

%code requires {
#include "AbstractSyntaxTree.h"
}

%define api.pure full
%define api.push-pull push
%define api.value.union.name SemanticValue
%define parse.error detailed
%locations

%union {
    char *text;
    int token;
    int dimension;
    AstExpression *expression;
    AstStatement *statement;
    AstBound *bound;
    RelationKind relation;
    ObjectiveKind objective;
    Program *program;
}

%token <text> IDENTIFIER NUMBER STRING
%token <token> LESS_EQUAL GREATER_EQUAL EQUAL_EQUAL MAX MIN PRINT SILENT
%token <token> CONST FLOAT INT VECTOR MATRIX BINARY
%token <token> ASSIGN PLUS MINUS MULT SLASH
%token <token> LPAREN RPAREN LBRACKET RBRACKET COMA EXCLAMACION
%token <token> IGNORED UNKNOWN NEWLINE

%type <program> program
%type <statement> statements statement
%type <expression> expression
%type <bound> optionalBound
%type <relation> relationOperator
%type <objective> objectiveOperator

%destructor { free($$); } <text>
%destructor { destroyAstExpression($$); } <expression>
%destructor { destroyAstStatement($$); } <statement>
%destructor { destroyAstBound($$); } <bound>

%left PLUS MINUS
%left MULT SLASH
%precedence UNARY_SIGN
%left LBRACKET

%start program

%%

program:
    leadingNewlines statements {
        $$ = acceptProgram($2);
    }
    ;

leadingNewlines:
      %empty
    | leadingNewlines NEWLINE
    ;

statements:
      statement {
        $$ = $1;
    }
    | statements NEWLINE statement {
        astAppend($1, $3);
        $$ = $1;
    }
    | statements NEWLINE {
        $$ = $1;
    }
    ;

statement:
      FLOAT IDENTIFIER optionalBound {
        $$ = astDeclaration(VAR_FLOAT, $2, 0, 0, $3);
    }
    | INT IDENTIFIER optionalBound {
        $$ = astDeclaration(VAR_INT, $2, 0, 0, $3);
    }
    | BINARY IDENTIFIER optionalBound {
        $$ = astDeclaration(VAR_BINARY, $2, 0, 0, $3);
    }
    | VECTOR IDENTIFIER LBRACKET NUMBER RBRACKET optionalBound {
        $$ = astDeclaration(VAR_VECTOR, $2, parseDimension($4), 1, $6);
        free($4);
    }
    | MATRIX IDENTIFIER LBRACKET NUMBER COMA NUMBER RBRACKET optionalBound {
        $$ = astDeclaration(VAR_MATRIX, $2, parseDimension($4), parseDimension($6), $8);
        free($4);
        free($6);
    }
    | CONST IDENTIFIER ASSIGN expression {
        $$ = astConstant($2, $4);
    }
    | expression relationOperator expression {
        $$ = astRelation($1, $2, $3);
    }
    | objectiveOperator expression {
        $$ = astObjective($1, NULL, $2);
    }
    | IDENTIFIER ASSIGN objectiveOperator expression {
        $$ = astObjective($3, $1, $4);
    }
    | PRINT LPAREN expression RPAREN {
        $$ = astPrint($3, false);
    }
    | EXCLAMACION PRINT LPAREN expression RPAREN {
        $$ = astPrint($4, true);
    }
    | SILENT {
        $$ = astSilent();
    }
    ;

optionalBound:
      %empty {
        $$ = NULL;
    }
    | relationOperator expression {
        $$ = astBound($1, $2);
    }
    ;

relationOperator:
      LESS_EQUAL {
        $$ = REL_LE;
    }
    | GREATER_EQUAL {
        $$ = REL_GE;
    }
    | EQUAL_EQUAL {
        $$ = REL_EQ;
    }
    ;

objectiveOperator:
      MAX {
        $$ = OBJ_MAXIMIZE;
    }
    | MIN {
        $$ = OBJ_MINIMIZE;
    }
    ;

expression:
      NUMBER {
        $$ = astText(EXPR_NUMBER, $1);
    }
    | STRING {
        $$ = astText(EXPR_STRING, $1);
    }
    | IDENTIFIER {
        $$ = astText(EXPR_IDENTIFIER, $1);
    }
    | expression PLUS expression {
        $$ = astBinary(OP_ADD, $1, $3);
    }
    | expression MINUS expression {
        $$ = astBinary(OP_SUB, $1, $3);
    }
    | expression MULT expression {
        $$ = astBinary(OP_MUL, $1, $3);
    }
    | expression SLASH expression {
        $$ = astBinary(OP_DIV, $1, $3);
    }
    | MINUS expression %prec UNARY_SIGN {
        $$ = astUnary(OP_NEG, $2);
    }
    | PLUS expression %prec UNARY_SIGN {
        $$ = astUnary(OP_POS, $2);
    }
    | LPAREN expression RPAREN {
        $$ = $2;
    }
    | expression LBRACKET expression RBRACKET {
        $$ = astIndex($1, $3, NULL);
    }
    | expression LBRACKET expression COMA expression RBRACKET {
        $$ = astIndex($1, $3, $5);
    }
    | IDENTIFIER LPAREN expression RPAREN {
        $$ = astCall($1, $3, NULL);
    }
    | IDENTIFIER LPAREN expression COMA expression RPAREN {
        $$ = astCall($1, $3, $5);
    }
    ;

%%

void yyerror(const YYLTYPE *location, const char *message) {
    if (location) {
        fprintf(stderr, "Syntax error near line %d: %s\n",
                location->first_line, message);
    } else {
        fprintf(stderr, "Syntax error: %s\n", message);
    }
}
