#include "semantic.hpp"

#include <cstdio>
#include <map>
#include <string>
#include <vector>

namespace {

struct FuncInfo {
    std::string ret;
    std::vector<std::string> params;
    int line = 0;
};

class Checker {
public:
    int run(Node* program);

private:
    std::vector<std::map<std::string, std::string>> scopes;
    std::map<std::string, FuncInfo> funcs;
    std::string currentReturn;
    int loopDepth = 0;
    int errors = 0;

    void error(int line, const std::string& msg);
    void declare(Node* var);
    bool lookup(const std::string& name, std::string& type) const;

    void function(Node* f);
    void declaration(Node* d);
    void statement(Node* s);
    void condition(Node* e);
    std::string expression(Node* e);
    std::string value(Node* e);
    std::string binary(Node* e);
    std::string call(Node* e);
};

bool compatible(const std::string& a, const std::string& b) {
    if (a == "error" || b == "error") return true;
    if (a == "void" || b == "void") return false;
    return (a == "string") == (b == "string");
}

bool isComparison(const std::string& op) {
    return op == "==" || op == "!=" || op == "<" || op == ">" || op == "<=" || op == ">=";
}

void Checker::error(int line, const std::string& msg) {
    ++errors;
    fflush(stdout);
    fprintf(stderr, "Семантична помилка (рядок %d): %s\n", line, msg.c_str());
}

void Checker::declare(Node* var) {
    if (var->type == "void") {
        error(var->line, "змінна '" + var->name + "' не може мати тип void");
        return;
    }
    auto& scope = scopes.back();
    if (scope.count(var->name)) {
        error(var->line, "змінну '" + var->name + "' вже оголошено в цій області видимості");
        return;
    }
    if (scopes.size() == 1 && funcs.count(var->name)) {
        error(var->line, "ім'я '" + var->name + "' вже зайняте функцією");
        return;
    }
    scope[var->name] = var->type;
}

bool Checker::lookup(const std::string& name, std::string& type) const {
    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) {
            type = found->second;
            return true;
        }
    }
    return false;
}

int Checker::run(Node* program) {
    scopes.emplace_back();

    for (Node* item : program->kids) {
        if (item->kind != NodeKind::Function) continue;
        if (funcs.count(item->name)) {
            error(item->line, "функцію '" + item->name + "' вже визначено");
            continue;
        }
        FuncInfo info;
        info.ret = item->type;
        info.line = item->line;
        for (Node* p : item->kids[0]->kids) info.params.push_back(p->type);
        funcs[item->name] = info;
    }

    for (Node* item : program->kids) {
        if (item->kind == NodeKind::DeclStmt) declaration(item);
        else if (item->kind == NodeKind::Function) function(item);
    }

    auto main = funcs.find("main");
    if (main == funcs.end())
        error(1, "не знайдено функцію main");
    else if (main->second.ret != "int")
        error(main->second.line, "функція main має повертати int");
    else if (!main->second.params.empty())
        error(main->second.line, "функція main не повинна мати параметрів");

    return errors;
}

void Checker::function(Node* f) {
    currentReturn = f->type;
    scopes.emplace_back();
    for (Node* p : f->kids[0]->kids) declare(p);
    for (Node* s : f->kids[1]->kids) statement(s);
    scopes.pop_back();
}

void Checker::declaration(Node* d) {
    for (Node* var : d->kids) {
        if (!var->kids.empty()) {
            std::string t = value(var->kids[0]);
            if (!compatible(var->type, t))
                error(var->line, "неможливо ініціалізувати змінну типу " + var->type + " значенням типу " + t);
        }
        declare(var);
    }
}

void Checker::condition(Node* e) {
    std::string t = value(e);
    if (t == "string")
        error(e->line, "умова має бути числом або bool, а не string");
}

void Checker::statement(Node* s) {
    switch (s->kind) {
        case NodeKind::DeclStmt:
            declaration(s);
            break;
        case NodeKind::ExprStmt:
            expression(s->kids[0]);
            break;
        case NodeKind::Block:
            scopes.emplace_back();
            for (Node* k : s->kids) statement(k);
            scopes.pop_back();
            break;
        case NodeKind::If:
            condition(s->kids[0]);
            statement(s->kids[1]);
            if (s->kids.size() > 2) statement(s->kids[2]);
            break;
        case NodeKind::While:
            condition(s->kids[0]);
            ++loopDepth;
            statement(s->kids[1]);
            --loopDepth;
            break;
        case NodeKind::For:
            scopes.emplace_back();
            statement(s->kids[0]);
            if (s->kids[1]->kind != NodeKind::Empty) condition(s->kids[1]);
            if (s->kids[2]->kind != NodeKind::Empty) expression(s->kids[2]);
            ++loopDepth;
            statement(s->kids[3]);
            --loopDepth;
            scopes.pop_back();
            break;
        case NodeKind::Return:
            if (s->kids.empty()) {
                if (currentReturn != "void")
                    error(s->line, "функція має повертати значення типу " + currentReturn);
            } else {
                std::string t = value(s->kids[0]);
                if (currentReturn == "void")
                    error(s->line, "функція типу void не може повертати значення");
                else if (!compatible(currentReturn, t))
                    error(s->line, "неможливо повернути " + t + " з функції типу " + currentReturn);
            }
            break;
        case NodeKind::Break:
            if (loopDepth == 0) error(s->line, "break поза циклом");
            break;
        case NodeKind::Continue:
            if (loopDepth == 0) error(s->line, "continue поза циклом");
            break;
        case NodeKind::Cout:
            for (Node* k : s->kids)
                if (k->kind != NodeKind::Endl) value(k);
            break;
        case NodeKind::Cin:
            for (Node* k : s->kids) expression(k);
            break;
        default:
            break;
    }
}

std::string Checker::value(Node* e) {
    std::string t = expression(e);
    if (t == "void") {
        error(e->line, "вираз типу void не має значення");
        return "error";
    }
    return t;
}

std::string Checker::expression(Node* e) {
    switch (e->kind) {
        case NodeKind::IntLit:    return "int";
        case NodeKind::DoubleLit: return "double";
        case NodeKind::StringLit: return "string";
        case NodeKind::BoolLit:   return "bool";

        case NodeKind::Ident: {
            std::string t;
            if (lookup(e->name, t)) return t;
            if (funcs.count(e->name)) error(e->line, "'" + e->name + "' є функцією, а не змінною");
            else error(e->line, "змінну '" + e->name + "' не оголошено");
            return "error";
        }

        case NodeKind::Assign: {
            std::string target = expression(e->kids[0]);
            std::string t = value(e->kids[1]);
            if (!compatible(target, t))
                error(e->line, "неможливо присвоїти значення типу " + t + " змінній типу " + target);
            else if (target == "string" && e->name != "=" && e->name != "+=")
                error(e->line, "оператор " + e->name + " не застосовний до string");
            return target;
        }

        case NodeKind::Binary:
            return binary(e);

        case NodeKind::Unary: {
            std::string t = value(e->kids[0]);
            if (t == "error") return t;
            if (t == "string") {
                error(e->line, "оператор " + e->name + " не застосовний до string");
                return "error";
            }
            if (e->name == "!") return "bool";
            return t == "double" ? "double" : "int";
        }

        case NodeKind::IncDec: {
            std::string t = expression(e->kids[0]);
            if (t == "string") {
                error(e->line, "оператор " + e->name + " не застосовний до string");
                return "error";
            }
            return t;
        }

        case NodeKind::Call:
            return call(e);

        default:
            return "error";
    }
}

std::string Checker::binary(Node* e) {
    std::string a = value(e->kids[0]);
    std::string b = value(e->kids[1]);
    const std::string& op = e->name;
    bool logical = op == "&&" || op == "||";

    if (a == "error" || b == "error")
        return (logical || isComparison(op)) ? "bool" : "error";

    if (logical) {
        if (a == "string" || b == "string")
            error(e->line, "оператор " + op + " не застосовний до string");
        return "bool";
    }

    if (isComparison(op)) {
        if (!compatible(a, b))
            error(e->line, "неможливо порівняти " + a + " і " + b);
        return "bool";
    }

    if (a == "string" || b == "string") {
        if (op == "+" && a == "string" && b == "string") return "string";
        error(e->line, "оператор " + op + " не застосовний до типів " + a + " і " + b);
        return "error";
    }

    if (op == "%") {
        if (a == "double" || b == "double") {
            error(e->line, "оператор % потребує цілих чисел");
            return "error";
        }
        return "int";
    }

    return (a == "double" || b == "double") ? "double" : "int";
}

std::string Checker::call(Node* e) {
    auto found = funcs.find(e->name);
    if (found == funcs.end()) {
        std::string t;
        if (lookup(e->name, t)) error(e->line, "'" + e->name + "' не є функцією");
        else error(e->line, "функцію '" + e->name + "' не оголошено");
        for (Node* arg : e->kids) expression(arg);
        return "error";
    }

    const FuncInfo& f = found->second;
    if (e->kids.size() != f.params.size())
        error(e->line, "функція '" + e->name + "' очікує " + std::to_string(f.params.size()) +
                       " аргумент(и), а передано " + std::to_string(e->kids.size()));

    for (size_t i = 0; i < e->kids.size(); ++i) {
        std::string t = value(e->kids[i]);
        if (i < f.params.size() && !compatible(f.params[i], t))
            error(e->kids[i]->line, "аргумент " + std::to_string(i + 1) + " функції '" + e->name +
                                    "': очікується " + f.params[i] + ", а передано " + t);
    }
    return f.ret;
}

}

int checkSemantics(Node* program) {
    Checker checker;
    return checker.run(program);
}
