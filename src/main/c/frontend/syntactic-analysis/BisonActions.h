#ifndef BISON_ACTIONS_HEADER
#define BISON_ACTIONS_HEADER
#include "../../support/type/CompilerState.h"
#include "../../support/type/ModuleDestructor.h"
#include "AbstractSyntaxTree.h"

ModuleDestructor initializeBisonActionsModule(CompilerState *compilerState);
Program *acceptProgram(AstStatement *statements);
#endif
