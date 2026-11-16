%{
#include "../../support/type/CompilationStatus.h"
#include "../../support/type/FlexContext.h"
#include "../../support/type/TokenLabel.h"
#include "../syntactic-analysis/AbstractSyntaxTree.h"
#include "../syntactic-analysis/BisonParser.h"

extern CompilationStatus EOFLexemeAction(void);
extern CompilationStatus IgnoredLexemeAction(void);
extern CompilationStatus TextLexemeAction(TokenLabel label);
extern CompilationStatus OperatorLexemeAction(TokenLabel label);
extern CompilationStatus UnknownLexemeAction(void);
extern CompilationStatus EnterMultilineCommentLexemeAction(FlexContext context);
extern CompilationStatus LeaveMultilineCommentLexemeAction(void);
%}
%x MULTILINE_COMMENT
%%
"/*"                             { return EnterMultilineCommentLexemeAction(MULTILINE_COMMENT); }
<MULTILINE_COMMENT>"*/"            { return LeaveMultilineCommentLexemeAction(); }
<MULTILINE_COMMENT>[^*]+            { return IgnoredLexemeAction(); }
<MULTILINE_COMMENT>.               { return IgnoredLexemeAction(); }

"#"[^\n]*                        { return IgnoredLexemeAction(); }
"//"[^\n]*                       { return IgnoredLexemeAction(); }
"<="                             { return OperatorLexemeAction(LESS_EQUAL); }
">="                             { return OperatorLexemeAction(GREATER_EQUAL); }
"=="                             { return OperatorLexemeAction(EQUAL_EQUAL); }
"="                              { return OperatorLexemeAction(ASSIGN); }
"max"                            { return OperatorLexemeAction(MAX); }
"min"                            { return OperatorLexemeAction(MIN); }
"print"                          { return OperatorLexemeAction(PRINT); }
"silent"                         { return OperatorLexemeAction(SILENT); }
"const"                          { return OperatorLexemeAction(CONST); }
"float"                          { return OperatorLexemeAction(FLOAT); }
"int"                            { return OperatorLexemeAction(INT); }
"vector"                         { return OperatorLexemeAction(VECTOR); }
"matrix"                         { return OperatorLexemeAction(MATRIX); }
"binary"                         { return OperatorLexemeAction(BINARY); }
"+"                              { return OperatorLexemeAction(PLUS); }
"-"                              { return OperatorLexemeAction(MINUS); }
"*"                              { return OperatorLexemeAction(MULT); }
"/"                              { return OperatorLexemeAction(SLASH); }
"("                              { return OperatorLexemeAction(LPAREN); }
")"                              { return OperatorLexemeAction(RPAREN); }
"["                              { return OperatorLexemeAction(LBRACKET); }
"]"                              { return OperatorLexemeAction(RBRACKET); }
","                              { return OperatorLexemeAction(COMA); }
"!"                              { return OperatorLexemeAction(EXCLAMACION); }
\"([^\"\\\n]|\\.)*\"             { return TextLexemeAction(STRING); }
([0-9]+(\.[0-9]+)?|\.[0-9]+)([eE][+-]?[0-9]+)? { return TextLexemeAction(NUMBER); }
[[:alpha:]_][[:alnum:]_]*         { return TextLexemeAction(IDENTIFIER); }
\r?\n                            { return OperatorLexemeAction(NEWLINE); }
[ \t\r\f\v]+                     { return IgnoredLexemeAction(); }
.                                { return UnknownLexemeAction(); }
<<EOF>>                          { return EOFLexemeAction(); }
%%
#include "FlexActions.h"
#include "FlexExport.h"
