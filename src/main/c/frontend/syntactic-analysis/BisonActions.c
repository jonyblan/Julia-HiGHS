#include "BisonActions.h"
#include "../../support/logging/Logger.h"

static CompilerState *state = NULL;
static Logger *logger = NULL;
static void shutdownBison(void) {
    if (logger) { destroyLogger(logger); logger = NULL; }
    state = NULL;
}
ModuleDestructor initializeBisonActionsModule(CompilerState *compilerState) {
    state = compilerState;
    logger = createLogger("BisonActions");
    return shutdownBison;
}
Program *acceptProgram(AstStatement *statements) {
    Program *program = astProgram(statements);
    if (state) state->abstractSyntaxtTree = program;
    return program;
}
