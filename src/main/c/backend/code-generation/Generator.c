#define _GNU_SOURCE
#include "Generator.h"
#include "../../support/logging/Logger.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static Logger *logger = NULL;
static void shutdownGenerator(void) {
    if (logger) { destroyLogger(logger); logger = NULL; }
}
ModuleDestructor initializeGeneratorModule(void) {
    logger = createLogger("Generator");
    return shutdownGenerator;
}

static void emitExpression(FILE *f, const AstExpression *expr) {
    switch (expr->kind) {
        case EXPR_NUMBER:
        case EXPR_IDENTIFIER:
            fputs(expr->text, f);
            break;
        case EXPR_STRING:
            /* Los strings llegan con comillas. Escapamos el $ que Julia interpretaria. */
            for (const char *p = expr->text; *p; ++p) {
                if (*p == '$') {
                    size_t backslashes = 0;
                    for (const char *q = p; q > expr->text && q[-1] == '\\'; --q) ++backslashes;
                    if (backslashes % 2 == 0) fputc('\\', f);
                }
                fputc(*p, f);
            }
            break;
        case EXPR_UNARY:
            fprintf(f, "(%c", expr->operation == OP_NEG ? '-' : '+');
            emitExpression(f, expr->left);
            fputc(')', f);
            break;
        case EXPR_BINARY: {
            const char *op = expr->operation == OP_ADD ? "+" :
                             expr->operation == OP_SUB ? "-" :
                             expr->operation == OP_MUL ? "*" : "/";
            fputc('(', f);
            emitExpression(f, expr->left);
            fprintf(f, " %s ", op);
            emitExpression(f, expr->right);
            fputc(')', f);
            break;
        }
        case EXPR_INDEX:
            fputc('(', f);
            emitExpression(f, expr->left);
            fputs(")[Int(", f);
            emitExpression(f, expr->right);
            fputc(')', f);
            if (expr->third) {
                fputs(", Int(", f);
                emitExpression(f, expr->third);
                fputc(')', f);
            }
            fputc(']', f);
            break;
        case EXPR_CALL:
            if (strcmp(expr->text, "matmul") == 0) {
                fputc('(', f);
                emitExpression(f, expr->left);
                fputs(" * ", f);
                emitExpression(f, expr->right);
                fputc(')', f);
            } else {
                const char *name = strcmp(expr->text, "trace") == 0 ? "tr" : expr->text;
                fprintf(f, "%s(", name);
                emitExpression(f, expr->left);
                if (expr->right) { fputs(", ", f); emitExpression(f, expr->right); }
                fputc(')', f);
            }
            break;
    }
}
static bool isObjectiveAlias(const Program *program, const AstExpression *expr) {
    if (expr->kind != EXPR_IDENTIFIER) return false;
    for (const AstStatement *s = program->first; s; s = s->next) {
        if (s->kind == STMT_OBJECTIVE && s->name && strcmp(s->name, expr->text) == 0)
            return true;
    }
    return false;
}
static bool isArrayExpression(const AstExpression *expr, const Program *program) {
    if (expr->kind == EXPR_IDENTIFIER) {
        for (const AstStatement *s = program->first; s; s = s->next)
            if (s->kind == STMT_DECLARATION && !strcmp(s->name, expr->text))
                return s->variableKind == VAR_VECTOR || s->variableKind == VAR_MATRIX;
    }
    if (expr->kind == EXPR_CALL && (!strcmp(expr->text, "cross") || !strcmp(expr->text, "matmul"))) return true;
    if (expr->kind == EXPR_UNARY) return isArrayExpression(expr->left, program);
    if (expr->kind == EXPR_BINARY) {
        return isArrayExpression(expr->left, program) || isArrayExpression(expr->right, program);
    }
    return false;
}
static bool isConstantExpression(const AstExpression *expr, const Program *program) {
    if (expr->kind == EXPR_NUMBER || expr->kind == EXPR_STRING) return true;
    if (expr->kind == EXPR_IDENTIFIER) {
        for (const AstStatement *s = program->first; s; s = s->next)
            if (s->kind == STMT_CONSTANT && !strcmp(s->name, expr->text)) return true;
    }
    if (expr->kind == EXPR_UNARY) return isConstantExpression(expr->left, program);
    if (expr->kind == EXPR_BINARY) return isConstantExpression(expr->left, program) && isConstantExpression(expr->right, program);
    return false;
}
static void emitJulia(FILE *f, const Program *program) {
    fputs("using JuMP\nusing HiGHS\nusing LinearAlgebra\nmodel = Model(HiGHS.Optimizer)\n", f);
    for (const AstStatement *s = program->first; s; s = s->next) {
        if (s->kind == STMT_CONSTANT) {
            fprintf(f, "const %s = ", s->name);
            emitExpression(f, s->left);
            fputc('\n', f);
        } else if (s->kind == STMT_DECLARATION) {
            fprintf(f, "@variable(model, %s", s->name);
            if (s->variableKind == VAR_VECTOR) fprintf(f, "[1:%d]", s->rows);
            else if (s->variableKind == VAR_MATRIX) fprintf(f, "[1:%d, 1:%d]", s->rows, s->cols);
            if (s->left) {
                fputs(s->relation == REL_LE ? " <= " : s->relation == REL_GE ? " >= " : " == ", f);
                emitExpression(f, s->left);
            }
            if (s->variableKind == VAR_INT) fputs(", Int", f);
            if (s->variableKind == VAR_BINARY) fputs(", Bin", f);
            fputs(")\n", f);
        } else if (s->kind == STMT_RELATION) {
            fputs("@constraint(model, ", f);
            emitExpression(f, s->left);
            fputs(s->relation == REL_LE ? " <= " : s->relation == REL_GE ? " >= " : " == ", f);
            emitExpression(f, s->right);
            fputs(")\n", f);
        } else if (s->kind == STMT_OBJECTIVE) {
            fprintf(f, "@objective(model, %s, ", s->objective == OBJ_MAXIMIZE ? "Max" : "Min");
            emitExpression(f, s->left);
            fputs(")\n", f);
        }
    }
    for (const AstStatement *s = program->first; s; s = s->next) {
        if (s->kind == STMT_SILENT || (s->kind == STMT_PRINT && s->suppressOutput)) {
            fputs("set_silent(model)\n", f);
            break;
        }
    }
    fputs("optimize!(model)\n", f);
    for (const AstStatement *s = program->first; s; s = s->next) {
        if (s->kind != STMT_PRINT) continue;
        const AstExpression *v = s->left;
        if (v->kind == EXPR_STRING || isConstantExpression(v, program)) {
            fputs("println(", f);
            emitExpression(f, v);
            fputs(")\n", f);
            continue;
        }
        fputs("if has_values(model)\n    println(", f);
        if (isObjectiveAlias(program, v)) {
            fputs("objective_value(model)", f);
        } else if (isArrayExpression(v, program)) {
            fputs("value.(", f);
            emitExpression(f, v);
            fputc(')', f);
        } else {
            fputs("value(", f);
            emitExpression(f, v);
            fputc(')', f);
        }
        fputs(")\nelse\n    println(\"No primal solution; status = \", termination_status(model))\nend\n", f);
    }
}
static bool writeBytes(const char *path, const char *text, size_t length) {
    FILE *file = fopen(path, "wb");
    if (!file) { perror(path); return false; }
    bool ok = fwrite(text, 1, length, file) == length;
    if (fclose(file) != 0) ok = false;
    if (!ok) perror(path);
    return ok;
}
static void writeJsonEscaped(FILE *f, const char *text, size_t length) {
    fputc('"', f);
    for (size_t i = 0; i < length; ++i) {
        unsigned char c = (unsigned char)text[i];
        if (c == '"' || c == '\\') { fputc('\\', f); fputc(c, f); }
        else if (c == '\n') fputs("\\n", f);
        else if (c == '\r') fputs("\\r", f);
        else if (c == '\t') fputs("\\t", f);
        else if (c < 32) fprintf(f, "\\u%04x", (unsigned)c);
        else fputc(c, f);
    }
    fputc('"', f);
}
static bool writeNotebook(const char *text, size_t length) {
    FILE *file = fopen("output/program.ipynb", "wb");
    if (!file) { perror("output/program.ipynb"); return false; }
    fputs("{\n  \"cells\": [{\"cell_type\": \"code\", \"execution_count\": null, \"metadata\": {}, \"outputs\": [], \"source\": [\n", file);
    size_t start = 0;
    for (size_t i = 0; i < length; ++i) {
        if (text[i] == '\n') {
            fputs("    ", file);
            writeJsonEscaped(file, text + start, i - start + 1);
            if (i + 1 < length) fputc(',', file);
            fputc('\n', file);
            start = i + 1;
        }
    }
    if (start < length) {
        fputs("    ", file);
        writeJsonEscaped(file, text + start, length - start);
        fputc('\n', file);
    }
    fputs("  ]}],\n  \"metadata\": {\"kernelspec\": {\"display_name\": \"Julia\", \"language\": \"julia\", \"name\": \"julia\"}, \"language_info\": {\"name\": \"julia\"}},\n  \"nbformat\": 4, \"nbformat_minor\": 4\n}\n", file);
    return fclose(file) == 0;
}
bool executeGenerator(CompilerState *state) {
    if (!state || !state->abstractSyntaxtTree) return false;
    if (mkdir("output", 0777) != 0 && errno != EEXIST) { perror("output"); return false; }
    char *text = NULL;
    size_t length = 0;
    FILE *stream = open_memstream(&text, &length);
    if (!stream) return false;
    emitJulia(stream, (const Program *)state->abstractSyntaxtTree);
    if (fclose(stream) != 0 || !text) { free(text); return false; }
    bool ok = writeBytes("output/program.txt", text, length) &&
              writeBytes("output/program.jl", text, length) &&
              writeNotebook(text, length);
    if (!ok) logError(logger, "Could not write every generated artifact.");
    free(text);
    return ok;
}
