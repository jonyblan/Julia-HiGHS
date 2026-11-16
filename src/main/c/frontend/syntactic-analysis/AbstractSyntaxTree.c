#include "AbstractSyntaxTree.h"
#include "../../support/logging/Logger.h"
#include <stdlib.h>

static Logger *logger = NULL;
static void shutdownAst(void) {
    if (logger != NULL) {
        destroyLogger(logger);
        logger = NULL;
    }
}
ModuleDestructor initializeAbstractSyntaxTreeModule(void) {
    logger = createLogger("AbstractSyntaxTree");
    return shutdownAst;
}
static AstExpression *newExpression(AstExpressionKind kind) {
    AstExpression *expression = calloc(1, sizeof(*expression));
    if (expression) expression->kind = kind;
    return expression;
}
AstExpression *astText(AstExpressionKind kind, char *text) {
    AstExpression *result = newExpression(kind);
    if (result) result->text = text;
    return result;
}
AstExpression *astUnary(AstOperator op, AstExpression *inner) {
    AstExpression *result = newExpression(EXPR_UNARY);
    if (result) { result->operation = op; result->left = inner; }
    return result;
}
AstExpression *astBinary(AstOperator op, AstExpression *lhs, AstExpression *rhs) {
    AstExpression *result = newExpression(EXPR_BINARY);
    if (result) { result->operation = op; result->left = lhs; result->right = rhs; }
    return result;
}
AstExpression *astIndex(AstExpression *array, AstExpression *row, AstExpression *col) {
    AstExpression *result = newExpression(EXPR_INDEX);
    if (result) { result->left = array; result->right = row; result->third = col; }
    return result;
}
AstExpression *astCall(char *function, AstExpression *first, AstExpression *second) {
    AstExpression *result = newExpression(EXPR_CALL);
    if (result) { result->text = function; result->left = first; result->right = second; }
    return result;
}
AstBound *astBound(RelationKind relation, AstExpression *expression) {
    AstBound *result = calloc(1, sizeof(*result));
    if (result) { result->relation = relation; result->expression = expression; }
    return result;
}
static AstStatement *newStatement(StatementKind kind) {
    AstStatement *result = calloc(1, sizeof(*result));
    if (result) result->kind = kind;
    return result;
}
AstStatement *astDeclaration(VariableKind kind, char *name, int rows, int cols, AstBound *bound) {
    AstStatement *result = newStatement(STMT_DECLARATION);
    if (result) {
        result->variableKind = kind;
        result->name = name;
        result->rows = rows;
        result->cols = cols;
        if (bound) { result->relation = bound->relation; result->left = bound->expression; free(bound); }
    }
    return result;
}
AstStatement *astRelation(AstExpression *lhs, RelationKind relation, AstExpression *rhs) {
    AstStatement *result = newStatement(STMT_RELATION);
    if (result) { result->left = lhs; result->right = rhs; result->relation = relation; }
    return result;
}
AstStatement *astConstant(char *name, AstExpression *value) {
    AstStatement *result = newStatement(STMT_CONSTANT);
    if (result) { result->name = name; result->left = value; }
    return result;
}
AstStatement *astObjective(ObjectiveKind objective, char *alias, AstExpression *value) {
    AstStatement *result = newStatement(STMT_OBJECTIVE);
    if (result) { result->objective = objective; result->name = alias; result->left = value; }
    return result;
}
AstStatement *astPrint(AstExpression *value, bool suppress) {
    AstStatement *result = newStatement(STMT_PRINT);
    if (result) { result->left = value; result->suppressOutput = suppress; }
    return result;
}
AstStatement *astSilent(void) { return newStatement(STMT_SILENT); }
void astAppend(AstStatement *head, AstStatement *tail) {
    if (!head) return;
    while (head->next) head = head->next;
    head->next = tail;
}
Program *astProgram(AstStatement *first) {
    Program *result = calloc(1, sizeof(*result));
    if (result) {
        result->first = first;
        for (AstStatement *s = first; s; s = s->next) result->last = s;
    }
    return result;
}
void destroyAstExpression(AstExpression *expr) {
    if (!expr) return;
    destroyAstExpression(expr->left);
    destroyAstExpression(expr->right);
    destroyAstExpression(expr->third);
    free(expr->text);
    free(expr);
}
void destroyAstBound(AstBound *bound) {
    if (!bound) return;
    destroyAstExpression(bound->expression);
    free(bound);
}
void destroyAstStatement(AstStatement *statement) {
    while (statement) {
        AstStatement *next = statement->next;
        destroyAstExpression(statement->left);
        destroyAstExpression(statement->right);
        free(statement->name);
        free(statement);
        statement = next;
    }
}
void destroyProgram(Program *program) {
    if (program) { destroyAstStatement(program->first); free(program); }
}
