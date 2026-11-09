#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>

#include "ast.hpp"
#include "parser.tab.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

extern FILE* yyin;
extern char* yytext;
extern int   yylineno;
extern int   syntaxErrors;
int yylex();
std::string tokenName(int token);

static void printTokens() {
    printf("Рядок  Токен                Текст            Значення\n");
    printf("------------------------------------------------------------\n");
    int count = 0;
    while (int tok = yylex()) {
        std::string value;
        if (tok == INT_LIT) {
            value = std::to_string(yylval.ival);
        } else if (tok == DOUBLE_LIT) {
            char b[32];
            snprintf(b, sizeof b, "%g", yylval.dval);
            value = b;
        } else if (tok == IDENT || tok == STRING_LIT) {
            value = visible(*yylval.sval);
            delete yylval.sval;
        }
        printf("%-6d %-20s %-16s %s\n", yylineno, tokenName(tok).c_str(), yytext, value.c_str());
        ++count;
    }
    printf("------------------------------------------------------------\n");
    printf("Усього токенів: %d\n", count);
}

static bool writeFile(const std::string& path, const std::string& text, const char* what) {
    std::ofstream f(path, std::ios::binary);
    if (!f) {
        fprintf(stderr, "Не вдалося записати файл: %s\n", path.c_str());
        return false;
    }
    f << text;
    printf("%s збережено у %s\n", what, path.c_str());
    return true;
}

static int usage() {
    fprintf(stderr,
        "Використання: minicpp <файл.cpp> [опції]\n"
        "  --tokens        показати токени\n"
        "  --ast           показати дерево AST\n"
        "  --json <файл>   зберегти AST у JSON\n");    return 1;
}

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    if (argc < 2) return usage();

    bool showTokens = false, showAst = false;
    std::string jsonPath;

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        bool hasValue = i + 1 < argc;
        if (arg == "--tokens") showTokens = true;
        else if (arg == "--ast") showAst = true;
        else if (arg == "--json" && hasValue) jsonPath = argv[++i];
        else {
            fprintf(stderr, "Невідома опція: %s\n", argv[i]);
            return usage();
        }
    }

    yyin = fopen(argv[1], "r");
    if (!yyin) {
        fprintf(stderr, "Не вдалося відкрити файл: %s\n", argv[1]);
        return 1;
    }

    if (showTokens) {
        printTokens();
        fclose(yyin);
        return 0;
    }

    int parsed = yyparse();
    fclose(yyin);
    if (parsed != 0 || syntaxErrors > 0) {
        fprintf(stderr, "Знайдено синтаксичних помилок: %d\n", syntaxErrors);
        return 1;
    }

    if (showAst) {
        printTree(astRoot);
        std::cout << std::endl;
    }
    if (!jsonPath.empty() && !writeFile(jsonPath, toJson(astRoot) + "\n", "AST"))
        return 1;

    if (!showAst && jsonPath.empty())
        printf("Аналіз успішний: помилок не знайдено.\n");

    delete astRoot;
    return 0;
}
