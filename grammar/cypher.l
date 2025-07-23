%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/ast.h"      // AST definitions first
#include "cypher.tab.h"      // BISON-generated header

// Track column for error reporting (yylineno handled by flex)
int yycolumn = 1;

#define YY_USER_ACTION \
    yycolumn += yyleng;

%}

%option noyywrap
%option yylineno

/* Define patterns */
IDENTIFIER  [a-zA-Z_][a-zA-Z0-9_]*
STRING      \"([^\"\\]|\\.)*\"
INTEGER     [+-]?[0-9]+
FLOAT       [+-]?[0-9]*\.[0-9]+([eE][+-]?[0-9]+)?
WHITESPACE  [ \t]+
NEWLINE     \n

%%

    /* Keywords */
"CREATE"        { yycolumn += yyleng; return CREATE; }
"MATCH"         { yycolumn += yyleng; return MATCH; }
"RETURN"        { yycolumn += yyleng; return RETURN; }
"WHERE"         { yycolumn += yyleng; return WHERE; }
"AND"           { yycolumn += yyleng; return AND; }
"OR"            { yycolumn += yyleng; return OR; }
"NOT"           { yycolumn += yyleng; return NOT; }
"IS"            { yycolumn += yyleng; return IS; }
"NULL"          { yycolumn += yyleng; return NULL_TOKEN; }
"true"          { yycolumn += yyleng; return TRUE; }
"false"         { yycolumn += yyleng; return FALSE; }

    /* Symbols */
"("             { yycolumn += yyleng; return LPAREN; }
")"             { yycolumn += yyleng; return RPAREN; }
"{"             { yycolumn += yyleng; return LBRACE; }
"}"             { yycolumn += yyleng; return RBRACE; }
"["             { yycolumn += yyleng; return LBRACKET; }
"]"             { yycolumn += yyleng; return RBRACKET; }
":"             { yycolumn += yyleng; return COLON; }
","             { yycolumn += yyleng; return COMMA; }
"."             { yycolumn += yyleng; return DOT; }
";"             { yycolumn += yyleng; return SEMICOLON; }
"->"            { yycolumn += yyleng; return ARROW_RIGHT; }
"<-"            { yycolumn += yyleng; return ARROW_LEFT; }
"-"             { yycolumn += yyleng; return DASH; }
"="             { yycolumn += yyleng; return EQ; }
"<>"            { yycolumn += yyleng; return NEQ; }
"<="            { yycolumn += yyleng; return LE; }
">="            { yycolumn += yyleng; return GE; }
"<"             { yycolumn += yyleng; return LT; }
">"             { yycolumn += yyleng; return GT; }

    /* Identifiers */
{IDENTIFIER}    { 
                    yycolumn += yyleng;
                    yylval.IDENTIFIER = strdup(yytext);
                    return IDENTIFIER;
                }

    /* Numeric literals */
{INTEGER}       {
                    yycolumn += yyleng;
                    yylval.INTEGER_LITERAL = strdup(yytext);
                    return INTEGER_LITERAL;
                }

{FLOAT}         {
                    yycolumn += yyleng;
                    yylval.FLOAT_LITERAL = strdup(yytext);
                    return FLOAT_LITERAL;
                }

    /* String literals */
{STRING}        {
                    yycolumn += yyleng;
                    // Remove quotes and store the content
                    char *str = malloc(yyleng - 1);
                    strncpy(str, yytext + 1, yyleng - 2);
                    str[yyleng - 2] = '\0';
                    yylval.STRING_LITERAL = str;
                    return STRING_LITERAL;
                }

    /* Whitespace */
{WHITESPACE}    { yycolumn += yyleng; /* ignore */ }

    /* Newlines */
{NEWLINE}       { yylineno++; yycolumn = 1; /* ignore */ }

    /* Anything else is an error */
.               { 
                    fprintf(stderr, "Unexpected character: %c at line %d, column %d\n", 
                            yytext[0], yylineno, yycolumn);
                    yycolumn += yyleng;
                }

%%

/* Initialize lexer with input string */
void init_lexer(const char *input) {
    yy_scan_string(input);
}

/* Cleanup lexer */
void cleanup_lexer(void) {
    yy_delete_buffer(YY_CURRENT_BUFFER);
}