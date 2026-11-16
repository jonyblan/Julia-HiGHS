#ifndef ABSTRACT_SYNTAX_TREE_HEADER
#define ABSTRACT_SYNTAX_TREE_HEADER

#include "../../support/type/ModuleDestructor.h"
#include <stddef.h>
#include <stdbool.h>

/* Nodos que usamos para representar el programa antes de generar Julia. */
typedef struct AstExpression AstExpression;
typedef struct AstStatement AstStatement;
typedef struct Program Program;

typedef enum {
    EXPR_NUMBER, EXPR_IDENTIFIER, EXPR_STRING, EXPR_UNARY,
    EXPR_BINARY, EXPR_INDEX, EXPR_CALL
} AstExpressionKind;

typedef enum {
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_NEG, OP_POS
} AstOperator;

struct AstExpression {
    AstExpressionKind kind;
    char *text;                 /* number spelling, identifier, string or function */
    AstOperator operation;
    AstExpression *left;
    AstExpression *right;
    AstExpression *third;      /* second matrix index or second call argument */
};

typedef enum {
    VAR_FLOAT, VAR_INT, VAR_VECTOR, VAR_MATRIX, VAR_BINARY
} VariableKind;

typedef enum {
    REL_LE, REL_GE, REL_EQ
} RelationKind;

typedef enum {
    STMT_DECLARATION, STMT_RELATION, STMT_CONSTANT,
    STMT_OBJECTIVE, STMT_PRINT, STMT_SILENT
} StatementKind;

typedef enum { OBJ_MAXIMIZE, OBJ_MINIMIZE } ObjectiveKind;

typedef struct {
    RelationKind relation;
    AstExpression *expression;  /* NULL means no bound */
} AstBound;

struct AstStatement {
    StatementKind kind;
    AstStatement *next;
    char *name;                 /* declaration/constant/objective alias */
    VariableKind variableKind;
    int rows;
    int cols;
    RelationKind relation;
    ObjectiveKind objective;
    bool suppressOutput;       /* !print(...) */
    AstExpression *left;       /* objective, print, constant, bound, lhs */
    AstExpression *right;      /* relation rhs */
};

struct Program {
    AstStatement *first;
    AstStatement *last;
};

ModuleDestructor initializeAbstractSyntaxTreeModule(void);
AstExpression *astText(AstExpressionKind kind, char *ownedText);
AstExpression *astUnary(AstOperator op, AstExpression *expr);
AstExpression *astBinary(AstOperator op, AstExpression *lhs, AstExpression *rhs);
AstExpression *astIndex(AstExpression *array, AstExpression *row, AstExpression *col);
AstExpression *astCall(char *ownedFunction, AstExpression *first, AstExpression *second);
AstBound *astBound(RelationKind relation, AstExpression *expression);
AstStatement *astDeclaration(VariableKind kind, char *ownedName, int rows, int cols, AstBound *bound);
AstStatement *astRelation(AstExpression *lhs, RelationKind relation, AstExpression *rhs);
AstStatement *astConstant(char *ownedName, AstExpression *value);
AstStatement *astObjective(ObjectiveKind objective, char *ownedAlias, AstExpression *value);
AstStatement *astPrint(AstExpression *value, bool suppress);
AstStatement *astSilent(void);
Program *astProgram(AstStatement *first);
void astAppend(AstStatement *head, AstStatement *tail);
void destroyAstExpression(AstExpression *expression);
void destroyAstStatement(AstStatement *statement);
void destroyAstBound(AstBound *bound);
void destroyProgram(Program *program);

#endif
