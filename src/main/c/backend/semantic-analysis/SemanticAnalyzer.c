#include "SemanticAnalyzer.h"
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Se arma una tabla de simbolos nueva para cada programa. */
typedef struct Symbol {
    const char *name;
    VariableKind kind;
    int rows, cols;
    bool constant;
    double value;
    struct Symbol *next;
} Symbol;

typedef enum { SH_INVALID, SH_SCALAR, SH_VECTOR, SH_MATRIX, SH_STRING } Shape;
typedef struct {
    Shape shape;
    int rows, cols;
    int degree;                 /* polynomial degree in decision variables */
    bool constant;
    double value;
} ExprInfo;

static int errors = 0;
static ExprInfo invalid(void) { return (ExprInfo){.shape = SH_INVALID}; }
static ExprInfo scalar(int degree, bool constant, double value) {
    return (ExprInfo){.shape = SH_SCALAR, .rows = 1, .cols = 1,
                      .degree = degree, .constant = constant, .value = value};
}
static ExprInfo array(Shape shape, int rows, int cols, int degree) {
    return (ExprInfo){.shape = shape, .rows = rows, .cols = cols, .degree = degree};
}
static void error(const char *message, const char *name) {
    fprintf(stderr, "Semantic error: ");
    fprintf(stderr, message, name ? name : "");
    fputc('\n', stderr);
    ++errors;
}
static const Symbol *lookup(const Symbol *symbols, const char *name) {
    for (const Symbol *s = symbols; s; s = s->next)
        if (strcmp(s->name, name) == 0) return s;
    return NULL;
}
static bool reservedName(const char *name) {
    const char *reserved[] = {"model", "JuMP", "HiGHS", "result", "objective", "value", "optimize", NULL};
    for (int i = 0; reserved[i]; ++i)
        if (strcmp(name, reserved[i]) == 0) return true;
    return false;
}
static bool addSymbol(Symbol **symbols, const char *name, VariableKind kind,
                      int rows, int cols, bool constant, double value) {
    if (lookup(*symbols, name) || reservedName(name)) {
        error("duplicate or reserved identifier '%s'", name);
        return false;
    }
    Symbol *s = calloc(1, sizeof(*s));
    if (!s) { error("out of memory for '%s'", name); return false; }
    s->name = name; s->kind = kind; s->rows = rows; s->cols = cols;
    s->constant = constant; s->value = value;
    s->next = *symbols; *symbols = s;
    return true;
}
static void freeSymbols(Symbol *symbols) {
    while (symbols) { Symbol *next = symbols->next; free(symbols); symbols = next; }
}
static bool equalShape(ExprInfo a, ExprInfo b) {
    return a.shape == b.shape && a.rows == b.rows && a.cols == b.cols;
}
static bool isNumeric(ExprInfo a) { return a.shape == SH_SCALAR; }
static ExprInfo analyzeExpr(const AstExpression *expr, const Symbol *symbols) {
    if (!expr) { error("missing expression: %s", ""); return invalid(); }
    if (expr->kind == EXPR_NUMBER) {
        char *end = NULL;
        errno = 0;
        double v = strtod(expr->text, &end);
        if (errno == ERANGE || !end || *end || !isfinite(v)) {
            error("invalid numeric literal '%s'", expr->text);
            return invalid();
        }
        return scalar(0, true, v);
    }
    if (expr->kind == EXPR_STRING) {
        /* El string ya incluye las comillas. */
        for (const char *s = expr->text + 1; *s && s[1]; ++s) {
            if (*s == '\\') {
                ++s;
                if (*s != 'n' && *s != 't' && *s != 'r' && *s != '"' && *s != '\\' && *s != '$') {
                    error("unsupported string escape in '%s'", expr->text);
                    return invalid();
                }
            }
        }
        return (ExprInfo){.shape = SH_STRING};
    }
    if (expr->kind == EXPR_IDENTIFIER) {
        const Symbol *s = lookup(symbols, expr->text);
        if (!s) {
            error("undeclared identifier '%s'", expr->text);
            return invalid();
        }
        if (s->constant) return scalar(0, true, s->value);
        if (s->kind == VAR_VECTOR) return array(SH_VECTOR, s->rows, 1, 1);
        if (s->kind == VAR_MATRIX) return array(SH_MATRIX, s->rows, s->cols, 1);
        return scalar(1, false, 0);
    }
    if (expr->kind == EXPR_UNARY) {
        ExprInfo v = analyzeExpr(expr->left, symbols);
        if (v.shape == SH_INVALID || v.shape == SH_STRING) {
            error("unary sign requires numeric value: '%s'", "");
            return invalid();
        }
        if (v.constant && expr->operation == OP_NEG) v.value = -v.value;
        return v;
    }
    if (expr->kind == EXPR_BINARY) {
        ExprInfo a = analyzeExpr(expr->left, symbols);
        ExprInfo b = analyzeExpr(expr->right, symbols);
        if (a.shape == SH_INVALID || b.shape == SH_INVALID) return invalid();
        if (a.shape == SH_STRING || b.shape == SH_STRING) {
            error("strings cannot be used in arithmetic: %s", ""); return invalid();
        }
        if (expr->operation == OP_ADD || expr->operation == OP_SUB) {
            if (!equalShape(a, b)) {
                error("incompatible shapes for addition/subtraction: %s", "");
                return invalid();
            }
            a.degree = a.degree > b.degree ? a.degree : b.degree;
            if (a.constant && b.constant) {
                a.value = expr->operation == OP_ADD ? a.value + b.value : a.value - b.value;
            } else a.constant = false;
            return a;
        }
        if (expr->operation == OP_DIV) {
            if (!b.constant || !isNumeric(b)) {
                error("division denominator must be a constant scalar: %s", ""); return invalid();
            }
            if (b.value == 0) { error("division by zero: %s", ""); return invalid(); }
            if (a.constant) a.value /= b.value;
            return a;
        }
        if (expr->operation == OP_MUL) {
            ExprInfo r = invalid();
            if (a.shape == SH_SCALAR && b.shape == SH_SCALAR) r = scalar(a.degree+b.degree, a.constant&&b.constant, a.value*b.value);
            else if (a.shape == SH_SCALAR && b.shape != SH_SCALAR) r = array(b.shape, b.rows, b.cols, a.degree+b.degree);
            else if (b.shape == SH_SCALAR && a.shape != SH_SCALAR) r = array(a.shape, a.rows, a.cols, a.degree+b.degree);
            else if (a.shape == SH_MATRIX && b.shape == SH_MATRIX && a.cols == b.rows) r = array(SH_MATRIX, a.rows, b.cols, a.degree+b.degree);
            else if (a.shape == SH_MATRIX && b.shape == SH_VECTOR && a.cols == b.rows) r = array(SH_VECTOR, a.rows, 1, a.degree+b.degree);
            if (r.shape == SH_INVALID) error("invalid operands/dimensions for multiplication: %s", "");
            return r;
        }
    }
    if (expr->kind == EXPR_INDEX) {
        ExprInfo src = analyzeExpr(expr->left, symbols);
        ExprInfo idx = analyzeExpr(expr->right, symbols);
        ExprInfo jdx = expr->third ? analyzeExpr(expr->third, symbols) : scalar(0, true, 1);
        if (src.shape != SH_VECTOR && src.shape != SH_MATRIX) {
            error("indexing requires a vector or matrix: %s", ""); return invalid();
        }
        if (!idx.constant || !isNumeric(idx) || !isfinite(idx.value) || floor(idx.value) != idx.value || idx.value < 1 || idx.value > src.rows ||
            !jdx.constant || !isNumeric(jdx) || !isfinite(jdx.value) || floor(jdx.value) != jdx.value || jdx.value < 1 || jdx.value > src.cols ||
            (src.shape == SH_MATRIX && !expr->third) || (src.shape == SH_VECTOR && expr->third)) {
            error("invalid or out-of-range constant index: %s", ""); return invalid();
        }
        return scalar(src.degree, false, 0);
    }
    if (expr->kind == EXPR_CALL) {
        ExprInfo a = analyzeExpr(expr->left, symbols);
        ExprInfo b = expr->right ? analyzeExpr(expr->right, symbols) : invalid();
        if (a.shape == SH_INVALID) return invalid();
        if (strcmp(expr->text, "sum") == 0 && !expr->right) {
            if (a.shape != SH_VECTOR && a.shape != SH_MATRIX) {
                error("sum requires a vector or matrix: %s", "");
                return invalid();
            }
            return scalar(a.degree, false, 0);
        }
        if (strcmp(expr->text, "trace") == 0 && !expr->right) {
            if (a.shape != SH_MATRIX || a.rows != a.cols) { error("trace requires a square matrix: %s", ""); return invalid(); }
            return scalar(a.degree, false, 0);
        }
        if (strcmp(expr->text, "dot") == 0 && expr->right) {
            if (a.shape != SH_VECTOR || b.shape != SH_VECTOR || a.rows != b.rows) { error("dot requires equal-length vectors: %s", ""); return invalid(); }
            return scalar(a.degree+b.degree, false, 0);
        }
        if (strcmp(expr->text, "cross") == 0 && expr->right) {
            if (a.shape != SH_VECTOR || b.shape != SH_VECTOR || a.rows != 3 || b.rows != 3) { error("cross requires two 3-element vectors: %s", ""); return invalid(); }
            return array(SH_VECTOR, 3, 1, a.degree+b.degree);
        }
        if (strcmp(expr->text, "matmul") == 0 && expr->right) {
            if (a.shape != SH_MATRIX || b.shape != SH_MATRIX || a.cols != b.rows) { error("matmul requires compatible matrices: %s", ""); return invalid(); }
            return array(SH_MATRIX, a.rows, b.cols, a.degree+b.degree);
        }
        error("unknown or incorrectly called function '%s'", expr->text);
        return invalid();
    }
    error("unknown expression kind: %s", "");
    return invalid();
}

bool validateJuliaProgram(Program *program) {
    if (!program) return false;
    errors = 0;
    Symbol *symbols = NULL;
    int variables = 0, objectives = 0;
    const char *objectiveAlias = NULL;
    for (AstStatement *s = program->first; s; s = s->next) {
        if (s->kind == STMT_CONSTANT) {
            ExprInfo v = analyzeExpr(s->left, symbols);
            if (!isNumeric(v) || !v.constant) error("constant must be a numeric compile-time expression: '%s'", s->name);
            else addSymbol(&symbols, s->name, VAR_FLOAT, 1, 1, true, v.value);
            continue;
        }
        if (s->kind == STMT_RELATION && s->left->kind == EXPR_IDENTIFIER &&
            !lookup(symbols, s->left->text) && s->relation != REL_EQ) {
            /* La primera vez que aparece a <= 5 lo tomamos como declaracion. */
            ExprInfo rhs = analyzeExpr(s->right, symbols);
            if (isNumeric(rhs) && rhs.constant) {
                s->name = strdup(s->left->text);
                destroyAstExpression(s->left);
                s->left = s->right;
                s->right = NULL;
                s->kind = STMT_DECLARATION;
                s->variableKind = VAR_FLOAT;
                s->rows = 1; s->cols = 1;
            } else {
                error("implicit declaration must have a constant bound: '%s'", s->left->text);
                continue;
            }
        }
        if (s->kind == STMT_DECLARATION) {
            ++variables;
            if (s->variableKind == VAR_VECTOR || s->variableKind == VAR_MATRIX) {
                if (s->rows < 1 || s->cols < 1 || s->rows > 10000 || s->cols > 10000) {
                    error("invalid vector/matrix dimension for '%s'", s->name);
                    continue;
                }
            } else { s->rows = 1; s->cols = 1; }
            if (!s->left && s->variableKind != VAR_BINARY) {
                error("variable must have a lower/upper bound: '%s'", s->name);
                continue;
            }
            if (s->left) {
                ExprInfo bound = analyzeExpr(s->left, symbols);
                if (!isNumeric(bound) || !bound.constant) {
                    error("variable creation bound must be constant: '%s'", s->name);
                    continue;
                }
            }
            addSymbol(&symbols, s->name, s->variableKind, s->rows, s->cols, false, 0);
            continue;
        }
        if (s->kind == STMT_RELATION) {
            ExprInfo lhs = analyzeExpr(s->left, symbols);
            ExprInfo rhs = analyzeExpr(s->right, symbols);
            if (!isNumeric(lhs) || !isNumeric(rhs)) error("constraints must compare scalars: %s", "");
            else if (lhs.degree > 1 || rhs.degree > 1) error("HiGHS supports only linear constraints in this DSL: %s", "");
            continue;
        }
        if (s->kind == STMT_OBJECTIVE) {
            ++objectives;
            ExprInfo v = analyzeExpr(s->left, symbols);
            if (!isNumeric(v)) error("objective must be scalar: %s", "");
            else if (v.degree > 2) error("HiGHS objective supports only linear/quadratic expressions: %s", "");
            if (s->name) {
                if (lookup(symbols, s->name) || reservedName(s->name)) error("objective alias conflicts with another name '%s'", s->name);
                objectiveAlias = s->name;
            }
            continue;
        }
        if (s->kind == STMT_PRINT) {
            if (!objectives) error("print must come after the objective: %s", "");
            if (s->left->kind == EXPR_IDENTIFIER && objectiveAlias &&
                strcmp(s->left->text, objectiveAlias) == 0) continue;
            (void)analyzeExpr(s->left, symbols);
            continue;
        }
        if (s->kind == STMT_SILENT) continue;
    }
    if (variables == 0) error("at least one decision variable required: %s", "");
    if (objectives != 1) error("exactly one objective required: %s", "");
    freeSymbols(symbols);
    return errors == 0;
}
