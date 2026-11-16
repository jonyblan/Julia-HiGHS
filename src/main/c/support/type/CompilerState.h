#ifndef COMPILER_STATE_HEADER
#define COMPILER_STATE_HEADER
/* Shared frontend/backend compilation state. AST owned by the caller. */
typedef struct {
    void *abstractSyntaxtTree;
} CompilerState;
#endif
