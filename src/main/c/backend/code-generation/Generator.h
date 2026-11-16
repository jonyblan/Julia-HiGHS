#ifndef GENERATOR_HEADER
#define GENERATOR_HEADER
#include "../../frontend/syntactic-analysis/AbstractSyntaxTree.h"
#include "../../support/type/CompilerState.h"
#include "../../support/type/ModuleDestructor.h"
#include <stdbool.h>
ModuleDestructor initializeGeneratorModule(void);
/* Produces output/program.{txt,jl,ipynb}; returns false on I/O failure. */
bool executeGenerator(CompilerState *state);
#endif
