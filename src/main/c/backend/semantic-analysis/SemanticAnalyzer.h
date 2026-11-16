#ifndef SEMANTIC_ANALYZER_HEADER
#define SEMANTIC_ANALYZER_HEADER
#include "../../frontend/syntactic-analysis/AbstractSyntaxTree.h"
#include <stdbool.h>

/* Validates types, shapes, names, solver limitations and DSL structure.
 * It normalizes a first bare `x <= 5` into a scalar declaration. */
bool validateJuliaProgram(Program *program);
#endif
