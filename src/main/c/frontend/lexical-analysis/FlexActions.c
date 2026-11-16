#include "FlexActions.h"
#include <string.h>
#include <stdlib.h>

static LexicalAnalyzer *lexer = NULL;
static Logger *logger = NULL;
static bool logIgnored = false;
static void shutdownFlexActions(void) {
    if (logger) { destroyLogger(logger); logger = NULL; }
    lexer = NULL;
}
ModuleDestructor initializeFlexActionsModule(LexicalAnalyzer *lexicalAnalyzer) {
    lexer = lexicalAnalyzer;
    logger = createLogger("FlexActions");
    logIgnored = getBooleanOrDefault("LOG_IGNORED_LEXEMES", false);
    return shutdownFlexActions;
}
static CompilationStatus sendToken(Token *token) {
    CompilationStatus result = pushToken(lexer, token);
    destroyToken(token);  /* El parser se queda con el texto copiado. */
    return result;
}
CompilationStatus OperatorLexemeAction(TokenLabel label) {
    return sendToken(createToken(lexer, label));
}
CompilationStatus TextLexemeAction(TokenLabel label) {
    Token *token = createToken(lexer, label);
    if (!token) return OUT_OF_MEMORY;
    token->semanticValue->text = strdup(token->lexeme);
    if (!token->semanticValue->text) { destroyToken(token); return OUT_OF_MEMORY; }
    return sendToken(token);
}
CompilationStatus IgnoredLexemeAction(void) {
    if (logIgnored) logDebugging(logger, "Ignoring whitespace/comment.");
    return IN_PROGRESS;
}
CompilationStatus UnknownLexemeAction(void) {
    Token *token = createToken(lexer, UNKNOWN);
    logError(logger, "Unknown token '%s' on line %u.", token->lexeme, token->line);
    destroyToken(token);
    return FAILED;
}
CompilationStatus EnterMultilineCommentLexemeAction(FlexContext context) {
    enterLexicalAnalyzerContext(lexer, context);
    return IN_PROGRESS;
}
CompilationStatus LeaveMultilineCommentLexemeAction(void) {
    leaveLexicalAnalyzerContext(lexer);
    return IN_PROGRESS;
}
CompilationStatus EOFLexemeAction(void) {
    FlexContext context = currentLexicalAnalyzerContext(lexer);
    if (context != 0) {
        logError(logger, "Unclosed multiline comment.");
        return FAILED;
    }
    Token *token = createToken(lexer, 0);
    return sendToken(token);
}
