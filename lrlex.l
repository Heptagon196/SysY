%option noyywrap
%x C_COMMENT
%{
#include <ctype.h>
#include "lrparser.tab.h"
int line_cnt = 1;
int getVal(char* s, int system) {
    int len = strlen(s);
    int val = 0;
    for (int i = 0; i < len; i++) {
        val = val * system + (isdigit(s[i]) ? (s[i] - '0') : (islower(s[i]) ? (s[i] - 'a' + 10) : (s[i] - 'A' + 10)));
    }
    return val;
}
%}

%%
\r*\n    { line_cnt++; }
"//"[^\n]* {}
"/*"            { BEGIN(C_COMMENT); }
<C_COMMENT>"*/" { BEGIN(INITIAL); }
<C_COMMENT>\n   { }
<C_COMMENT>.    { }
[ \t] { /* ignore whitespace */ }
EOF { return END; }
"+" { return ADD; }
"-" { return SUB; }
"*"    { return MUL; }
"/"    { return DIV; }
"%" { return MOD; }
"const" { return CONST; }
"int" { return INT; }
"void" { return VOID; }
"if" { return IF; }
"else" { return ELSE; }
"while" { return WHILE; }
"break" { return BREAK; }
"continue" { return CONTINUE; }
"return" { return RETURN; }
"extern" { return EXTERN; }
0x[0-9a-eA-E]+ { yylval.iVal = getVal(yytext + 2, 16); return NUMBER; }
0[0-7]+ { yylval.iVal = getVal(yytext + 1, 16); return NUMBER; }
[0-9]+ { yylval.iVal = atoi(yytext); return NUMBER; }
[a-zA-Z_][a-zA-Z_0-9]* {
    strcpy(yylval.sVal, yytext);
    return SYMBOL;
}
"==" { return E; }
"=" { return ASSIGN; }
"," { return COM; }
";" { return SEM; }
"(" { return L_ROUND_BRACE; }
")" { return R_ROUND_BRACE; }
"[" { return L_SQUARE_BRACE; }
"]" { return R_SQUARE_BRACE; }
"{" { return L_CURLY_BRACE; }
"}" { return R_CURLY_BRACE; }
"!=" { return NE; }
">" { return G; }
">=" { return GE; }
"<" { return L; }
"<=" { return LE; }
"!" { return NOT; }
"&&" { return AND; }
"||" { return OR; }
.     { fprintf(stderr, "Mystery character %c\n", *yytext); }
%%
