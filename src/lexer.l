%option noyywrap
%option yylineno
%option nounput
%option noinput

%{
#include <cstdio>
#include <cstdlib>
#include <string>
#include "parser.tab.hpp"

#define YY_USER_ACTION yylloc.first_line = yylloc.last_line = yylineno;

static std::string unescape(const char* text) {
    std::string s;
    for (const char* p = text + 1; *p && *(p + 1); ++p) {
        if (*p == '\\') {
            ++p;
            switch (*p) {
                case 'n':  s += '\n'; break;
                case 't':  s += '\t'; break;
                case '\\': s += '\\'; break;
                case '"':  s += '"';  break;
                default:   s += *p;   break;
            }
        } else {
            s += *p;
        }
    }
    return s;
}

static void lexError(const char* msg) {
    fflush(stdout);
    fprintf(stderr, "Лексична помилка (рядок %d): %s\n", yylineno, msg);
    exit(1);
}
%}

%x COMMENT

DIGIT   [0-9]
ID      [A-Za-z_][A-Za-z0-9_]*
EXP     [eE][+-]?{DIGIT}+

%%

[ \t\r\n]+              { }
"//".*                  { }
"/*"                    { BEGIN(COMMENT); }
<COMMENT>"*/"           { BEGIN(INITIAL); }
<COMMENT>\n             { }
<COMMENT>.              { }
<COMMENT><<EOF>>        { lexError("незакритий коментар /* ..."); }
"#"[^\n]*               { }

"int"                   { return KW_INT; }
"double"                { return KW_DOUBLE; }
"bool"                  { return KW_BOOL; }
"void"                  { return KW_VOID; }
"std::"?"string"        { return KW_STRING; }
"if"                    { return KW_IF; }
"else"                  { return KW_ELSE; }
"while"                 { return KW_WHILE; }
"for"                   { return KW_FOR; }
"return"                { return KW_RETURN; }
"break"                 { return KW_BREAK; }
"continue"              { return KW_CONTINUE; }
"true"                  { return KW_TRUE; }
"false"                 { return KW_FALSE; }
"using"                 { return KW_USING; }
"namespace"             { return KW_NAMESPACE; }
"std::"?"cout"          { return KW_COUT; }
"std::"?"cin"           { return KW_CIN; }
"std::"?"endl"          { return KW_ENDL; }

{DIGIT}+"."{DIGIT}*{EXP}?   |
"."{DIGIT}+{EXP}?           |
{DIGIT}+{EXP}               { yylval.dval = atof(yytext); return DOUBLE_LIT; }

{DIGIT}+                    { yylval.ival = atoi(yytext); return INT_LIT; }

\"([^\\\"\n]|\\.)*\"        { yylval.sval = new std::string(unescape(yytext)); return STRING_LIT; }
\"([^\\\"\n]|\\.)*          { lexError("незакритий рядок \"..."); }

{ID}                    { yylval.sval = new std::string(yytext); return IDENT; }

"=="                    { return EQ; }
"!="                    { return NE; }
"<="                    { return LE; }
">="                    { return GE; }
"&&"                    { return AND; }
"||"                    { return OR; }
"++"                    { return INC; }
"--"                    { return DEC; }
"<<"                    { return SHL; }
">>"                    { return SHR; }
"+="                    { return ADD_ASSIGN; }
"-="                    { return SUB_ASSIGN; }
"*="                    { return MUL_ASSIGN; }
"/="                    { return DIV_ASSIGN; }

[-+*/%=<>!(){};,]       { return yytext[0]; }

.                       { char m[64]; snprintf(m, sizeof m, "невідомий символ '%s'", yytext); lexError(m); }

%%
