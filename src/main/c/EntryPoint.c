#include "backend/code-generation/Generator.h"
#include "backend/semantic-analysis/SemanticAnalyzer.h"
#include "frontend/Frontend.h"
#include "frontend/lexical-analysis/FlexActions.h"
#include "frontend/syntactic-analysis/BisonActions.h"
#include "support/logging/Logger.h"
#include "support/type/CompilationStatus.h"
#include "support/type/CompilerState.h"
#include "support/type/ModuleDestructor.h"
#include <stdio.h>

/**
 * The main entry-point of the entire application. If you use "strtok" to
 * parse anything inside this project instead of using Flex and Bison, I will
 * find you, and I will kill you (Bryan Mills; "Taken", 2008).
 */
int main(int length, const char **arguments) {
	/* Para no dejar resultados viejos si falla el parser. */
	remove("output/program.txt");
	remove("output/program.jl");
	remove("output/program.ipynb");
	LexicalAnalyzer * lexicalAnalyzer = createLexicalAnalyzer();
	Logger * logger = createLogger("EntryPoint");
	for (int k = 0; k < length; ++k) {
		logDebugging(logger, "Argument %d: \"%s\"", k, arguments[k]);
	}
	CompilerState compilerState = {
		.abstractSyntaxtTree = NULL
	};
	ModuleDestructor moduleDestructors[] = {
		initializeAbstractSyntaxTreeModule(),
		initializeFlexActionsModule(lexicalAnalyzer),
		initializeBisonActionsModule(&compilerState),
		initializeFrontendModule(lexicalAnalyzer),
		initializeGeneratorModule()
	};
	CompilationStatus compilationStatus = executeSyntacticAnalysis();
	Program * program = compilerState.abstractSyntaxtTree;
	if (compilationStatus == SUCCEEDED) {
        if (validateJuliaProgram(program)) {
            if (!executeGenerator(&compilerState)) {
                compilationStatus = FAILED;
            }
        } else {
            logError(logger, "Semantic analysis failed.");
            compilationStatus = FAILED;
        }
	} else {
        logError(logger, "Syntactic analysis failed.");
        compilationStatus = FAILED;
	}
	logDebugging(logger, "Releasing AST resources...");
	destroyProgram(program);
	for (int k = (sizeof(moduleDestructors)/sizeof(ModuleDestructor)) - 1; 0 <= k; --k) {
		moduleDestructors[k]();
	}
	logDebugging(logger, "Compilation is done.");
	destroyLogger(logger);
	destroyLexicalAnalyzer(lexicalAnalyzer);
	return compilationStatus;
}
