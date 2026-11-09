%code requires {
#include <string>
#include "ast.hpp"
}

%code {
#include <cstdio>
#include <string>

int yylex();
extern int   yylineno;
extern char* yytext;
void yyerror(const char* msg);

int syntaxErrors = 0;

static Node* named(NodeKind kind, int line, std::string* name) {
    Node* n = makeNode(kind, line, *name);
    delete name;
    return n;
}

static Node* typed(NodeKind kind, int line, std::string* type, std::string* name) {
    Node* n = named(kind, line, name);
    n->type = *type;
    delete type;
    return n;
}

static Node* binary(const char* op, Node* left, Node* right, int line) {
    return makeNode(NodeKind::Binary, line, op)->add(left)->add(right);
}

static Node* unary(const char* op, Node* operand, int line) {
    return makeNode(NodeKind::Unary, line, op)->add(operand);
}

static Node* incdec(const char* op, std::string* name, bool prefix, int line) {
    Node* n = makeNode(NodeKind::IncDec, line, op)->add(named(NodeKind::Ident, line, name));
    n->prefix = prefix;
    return n;
}
}

%define parse.error verbose
%token-table
%locations
%expect 0

%union {
    int          ival;
    double       dval;
    std::string* sval;
    const char*  op;
    Node*        node;
}

%token KW_INT "int" KW_DOUBLE "double" KW_BOOL "bool" KW_VOID "void" KW_STRING "string"
%token KW_IF "if" KW_ELSE "else" KW_WHILE "while" KW_FOR "for"
%token KW_RETURN "return" KW_BREAK "break" KW_CONTINUE "continue"
%token KW_TRUE "true" KW_FALSE "false"
%token KW_USING "using" KW_NAMESPACE "namespace"
%token KW_COUT "cout" KW_CIN "cin" KW_ENDL "endl"

%token <sval> IDENT      "identifier"
%token <ival> INT_LIT    "integer literal"
%token <dval> DOUBLE_LIT "double literal"
%token <sval> STRING_LIT "string literal"

%token EQ "==" NE "!=" LE "<=" GE ">=" AND "&&" OR "||"
%token INC "++" DEC "--" SHL "<<" SHR ">>"
%token ADD_ASSIGN "+=" SUB_ASSIGN "-=" MUL_ASSIGN "*=" DIV_ASSIGN "/="

%type <sval> type
%type <op>   assign_op
%type <node> program top_list top_item using_decl func_def params_opt param_list param
%type <node> var_decl var_init_list var_init block stmt_list stmt
%type <node> if_stmt while_stmt for_stmt for_init return_stmt
%type <node> cout_stmt out_list out_item cin_stmt in_list
%type <node> expr_opt expr primary args_opt arg_list

%right '=' ADD_ASSIGN SUB_ASSIGN MUL_ASSIGN DIV_ASSIGN
%left  OR
%left  AND
%left  EQ NE
%left  '<' '>' LE GE
%left  '+' '-'
%left  '*' '/' '%'
%right '!' UMINUS

%nonassoc LOWER_THAN_ELSE
%nonassoc KW_ELSE

%start program

%%

program
    : top_list                       { astRoot = $1; $$ = $1; }
    ;

top_list
    : %empty                         { $$ = makeNode(NodeKind::Program, 1); }
    | top_list top_item              { $$ = $1->add($2); }
    ;

top_item
    : using_decl
    | func_def
    | var_decl
    ;

using_decl
    : KW_USING KW_NAMESPACE IDENT ';'    { $$ = named(NodeKind::Using, @1.first_line, $3); }
    ;

type
    : KW_INT                         { $$ = new std::string("int"); }
    | KW_DOUBLE                      { $$ = new std::string("double"); }
    | KW_BOOL                        { $$ = new std::string("bool"); }
    | KW_STRING                      { $$ = new std::string("string"); }
    | KW_VOID                        { $$ = new std::string("void"); }
    ;

func_def
    : type IDENT '(' params_opt ')' block
        { $$ = typed(NodeKind::Function, @2.first_line, $1, $2)->add($4)->add($6); }
    ;

params_opt
    : %empty                         { $$ = makeNode(NodeKind::Params, @$.first_line); }
    | param_list
    ;

param_list
    : param                          { $$ = makeNode(NodeKind::Params, @1.first_line)->add($1); }
    | param_list ',' param           { $$ = $1->add($3); }
    ;

param
    : type IDENT                     { $$ = typed(NodeKind::Param, @2.first_line, $1, $2); }
    ;

var_decl
    : type var_init_list ';'
        {
            $$ = $2;
            $$->type = *$1;
            $$->line = @1.first_line;
            for (Node* v : $$->kids) v->type = *$1;
            delete $1;
        }
    ;

var_init_list
    : var_init                       { $$ = makeNode(NodeKind::DeclStmt, @1.first_line)->add($1); }
    | var_init_list ',' var_init     { $$ = $1->add($3); }
    ;

var_init
    : IDENT                          { $$ = named(NodeKind::VarDecl, @1.first_line, $1); }
    | IDENT '=' expr                 { $$ = named(NodeKind::VarDecl, @1.first_line, $1)->add($3); }
    ;

block
    : '{' stmt_list '}'              { $$ = $2; $$->line = @1.first_line; }
    ;

stmt_list
    : %empty                         { $$ = makeNode(NodeKind::Block, @$.first_line); }
    | stmt_list stmt                 { $$ = $1->add($2); }
    ;

stmt
    : var_decl
    | expr ';'                       { $$ = makeNode(NodeKind::ExprStmt, @1.first_line)->add($1); }
    | ';'                            { $$ = makeNode(NodeKind::Empty, @1.first_line); }
    | block
    | if_stmt
    | while_stmt
    | for_stmt
    | return_stmt
    | KW_BREAK ';'                   { $$ = makeNode(NodeKind::Break, @1.first_line); }
    | KW_CONTINUE ';'                { $$ = makeNode(NodeKind::Continue, @1.first_line); }
    | cout_stmt
    | cin_stmt
    | error ';'                      { $$ = makeNode(NodeKind::Empty, @$.first_line); }
    ;

if_stmt
    : KW_IF '(' expr ')' stmt                %prec LOWER_THAN_ELSE
        { $$ = makeNode(NodeKind::If, @1.first_line)->add($3)->add($5); }
    | KW_IF '(' expr ')' stmt KW_ELSE stmt
        { $$ = makeNode(NodeKind::If, @1.first_line)->add($3)->add($5)->add($7); }
    ;

while_stmt
    : KW_WHILE '(' expr ')' stmt     { $$ = makeNode(NodeKind::While, @1.first_line)->add($3)->add($5); }
    ;

for_stmt
    : KW_FOR '(' for_init expr_opt ';' expr_opt ')' stmt
        { $$ = makeNode(NodeKind::For, @1.first_line)->add($3)->add($4)->add($6)->add($8); }
    ;

for_init
    : var_decl
    | expr_opt ';'
        {
            if ($1->kind == NodeKind::Empty) $$ = $1;
            else $$ = makeNode(NodeKind::ExprStmt, @1.first_line)->add($1);
        }
    ;

return_stmt
    : KW_RETURN expr_opt ';'
        {
            $$ = makeNode(NodeKind::Return, @1.first_line);
            if ($2->kind == NodeKind::Empty) delete $2;
            else $$->add($2);
        }
    ;

cout_stmt
    : KW_COUT out_list ';'           { $$ = $2; $$->line = @1.first_line; }
    ;

out_list
    : SHL out_item                   { $$ = makeNode(NodeKind::Cout, @1.first_line)->add($2); }
    | out_list SHL out_item          { $$ = $1->add($3); }
    ;

out_item
    : expr
    | KW_ENDL                        { $$ = makeNode(NodeKind::Endl, @1.first_line); }
    ;

cin_stmt
    : KW_CIN in_list ';'             { $$ = $2; $$->line = @1.first_line; }
    ;

in_list
    : SHR IDENT                      { $$ = makeNode(NodeKind::Cin, @1.first_line)->add(named(NodeKind::Ident, @2.first_line, $2)); }
    | in_list SHR IDENT              { $$ = $1->add(named(NodeKind::Ident, @3.first_line, $3)); }
    ;

expr_opt
    : %empty                         { $$ = makeNode(NodeKind::Empty, @$.first_line); }
    | expr
    ;

expr
    : IDENT assign_op expr  %prec '='
        { $$ = makeNode(NodeKind::Assign, @1.first_line, $2)->add(named(NodeKind::Ident, @1.first_line, $1))->add($3); }
    | expr OR expr                   { $$ = binary("||", $1, $3, @2.first_line); }
    | expr AND expr                  { $$ = binary("&&", $1, $3, @2.first_line); }
    | expr EQ expr                   { $$ = binary("==", $1, $3, @2.first_line); }
    | expr NE expr                   { $$ = binary("!=", $1, $3, @2.first_line); }
    | expr '<' expr                  { $$ = binary("<",  $1, $3, @2.first_line); }
    | expr '>' expr                  { $$ = binary(">",  $1, $3, @2.first_line); }
    | expr LE expr                   { $$ = binary("<=", $1, $3, @2.first_line); }
    | expr GE expr                   { $$ = binary(">=", $1, $3, @2.first_line); }
    | expr '+' expr                  { $$ = binary("+",  $1, $3, @2.first_line); }
    | expr '-' expr                  { $$ = binary("-",  $1, $3, @2.first_line); }
    | expr '*' expr                  { $$ = binary("*",  $1, $3, @2.first_line); }
    | expr '/' expr                  { $$ = binary("/",  $1, $3, @2.first_line); }
    | expr '%' expr                  { $$ = binary("%",  $1, $3, @2.first_line); }
    | '!' expr                       { $$ = unary("!", $2, @1.first_line); }
    | '-' expr          %prec UMINUS { $$ = unary("-", $2, @1.first_line); }
    | '+' expr          %prec UMINUS { $$ = unary("+", $2, @1.first_line); }
    | INC IDENT                      { $$ = incdec("++", $2, true,  @1.first_line); }
    | DEC IDENT                      { $$ = incdec("--", $2, true,  @1.first_line); }
    | IDENT INC                      { $$ = incdec("++", $1, false, @1.first_line); }
    | IDENT DEC                      { $$ = incdec("--", $1, false, @1.first_line); }
    | primary
    ;

assign_op
    : '='                            { $$ = "="; }
    | ADD_ASSIGN                     { $$ = "+="; }
    | SUB_ASSIGN                     { $$ = "-="; }
    | MUL_ASSIGN                     { $$ = "*="; }
    | DIV_ASSIGN                     { $$ = "/="; }
    ;

primary
    : INT_LIT                        { $$ = makeNode(NodeKind::IntLit, @1.first_line); $$->ival = $1; }
    | DOUBLE_LIT                     { $$ = makeNode(NodeKind::DoubleLit, @1.first_line); $$->dval = $1; }
    | STRING_LIT                     { $$ = named(NodeKind::StringLit, @1.first_line, $1); }
    | KW_TRUE                        { $$ = makeNode(NodeKind::BoolLit, @1.first_line); $$->ival = 1; }
    | KW_FALSE                       { $$ = makeNode(NodeKind::BoolLit, @1.first_line); $$->ival = 0; }
    | IDENT                          { $$ = named(NodeKind::Ident, @1.first_line, $1); }
    | IDENT '(' args_opt ')'
        {
            $$ = named(NodeKind::Call, @1.first_line, $1);
            $$->kids = $3->kids;
            $3->kids.clear();
            delete $3;
        }
    | '(' expr ')'                   { $$ = $2; }
    ;

args_opt
    : %empty                         { $$ = makeNode(NodeKind::Args, @$.first_line); }
    | arg_list
    ;

arg_list
    : expr                           { $$ = makeNode(NodeKind::Args, @1.first_line)->add($1); }
    | arg_list ',' expr              { $$ = $1->add($3); }
    ;

%%

void yyerror(const char* msg) {
    ++syntaxErrors;
    fprintf(stderr, "Синтаксична помилка (рядок %d, біля '%s'): %s\n", yylloc.first_line, yytext, msg);
}

std::string tokenName(int token) {
    if (token > 0 && token < 256)
        return std::string("'") + (char)token + "'";
    std::string name = yytname[YYTRANSLATE(token)];
    if (name.size() >= 2 && name.front() == '"')
        name = name.substr(1, name.size() - 2);
    return name;
}
