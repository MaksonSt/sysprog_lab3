#include "interpreter.hpp"

#include <cstdio>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Value {
    enum Type { Int, Double, Bool, String, Void };

    Type t = Void;
    long long i = 0;
    double d = 0;
    std::string s;

    static Value ofInt(long long v)           { Value x; x.t = Int;    x.i = v;          return x; }
    static Value ofDouble(double v)           { Value x; x.t = Double; x.d = v;          return x; }
    static Value ofBool(bool v)               { Value x; x.t = Bool;   x.i = v ? 1 : 0;  return x; }
    static Value ofString(const std::string& v) { Value x; x.t = String; x.s = v;        return x; }

    long long asInt() const  { return t == Double ? (long long)d : i; }
    double asDouble() const  { return t == Double ? d : (double)i; }
    bool asBool() const      { return t == Double ? d != 0 : i != 0; }
};

struct RuntimeError : std::runtime_error {
    int line;
    RuntimeError(int l, const std::string& msg) : std::runtime_error(msg), line(l) {}
};

Value convert(const Value& v, const std::string& type) {
    if (type == "int")    return Value::ofInt(v.asInt());
    if (type == "double") return Value::ofDouble(v.asDouble());
    if (type == "bool")   return Value::ofBool(v.asBool());
    if (type == "string") return Value::ofString(v.s);
    return Value();
}

std::string typeName(Value::Type t) {
    static const char* names[] = {"int", "double", "bool", "string", "void"};
    return names[t];
}

enum class Flow { Normal, Break, Continue, Return };

class Interpreter {
public:
    int run(Node* program);

private:
    std::map<std::string, Node*> funcs;
    std::map<std::string, Value> globals;
    std::vector<std::map<std::string, Value>> scopes;
    Value returnValue;
    int depth = 0;

    Value& variable(const std::string& name, int line);
    void declare(const std::string& name, const Value& v);
    void declaration(Node* d);
    Flow execute(Node* s);
    Value evaluate(Node* e);
    Value binary(const std::string& op, const Value& a, const Value& b, int line);
    Value invoke(Node* f, const std::vector<Value>& args, int line);
    void read(Value& target);
    void print(const Value& v);
};

Value& Interpreter::variable(const std::string& name, int line) {
    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) return found->second;
    }
    auto g = globals.find(name);
    if (g != globals.end()) return g->second;
    throw RuntimeError(line, "невідома змінна '" + name + "'");
}

void Interpreter::declare(const std::string& name, const Value& v) {
    if (scopes.empty()) globals[name] = v;
    else scopes.back()[name] = v;
}

void Interpreter::declaration(Node* d) {
    for (Node* var : d->kids) {
        Value init = var->kids.empty() ? Value::ofInt(0) : evaluate(var->kids[0]);
        declare(var->name, convert(init, var->type));
    }
}

int Interpreter::run(Node* program) {
    try {
        for (Node* item : program->kids)
            if (item->kind == NodeKind::Function) funcs[item->name] = item;
        for (Node* item : program->kids)
            if (item->kind == NodeKind::DeclStmt) declaration(item);

        Value result = invoke(funcs.at("main"), {}, funcs.at("main")->line);
        std::cout.flush();
        return (int)result.asInt();
    } catch (const RuntimeError& e) {
        std::cout.flush();
        fprintf(stderr, "Помилка виконання (рядок %d): %s\n", e.line, e.what());
        return 1;
    }
}

Value Interpreter::invoke(Node* f, const std::vector<Value>& args, int line) {
    if (++depth > 5000) throw RuntimeError(line, "занадто глибока рекурсія");

    std::vector<std::map<std::string, Value>> saved = std::move(scopes);
    scopes.clear();
    scopes.emplace_back();

    const std::vector<Node*>& params = f->kids[0]->kids;
    for (size_t i = 0; i < params.size(); ++i)
        scopes.back()[params[i]->name] = convert(args[i], params[i]->type);

    Flow flow = Flow::Normal;
    for (Node* s : f->kids[1]->kids) {
        flow = execute(s);
        if (flow == Flow::Return) break;
    }

    Value result = convert(flow == Flow::Return ? returnValue : Value::ofInt(0), f->type);
    scopes = std::move(saved);
    --depth;
    return result;
}

Flow Interpreter::execute(Node* s) {
    switch (s->kind) {
        case NodeKind::DeclStmt:
            declaration(s);
            return Flow::Normal;

        case NodeKind::ExprStmt:
            evaluate(s->kids[0]);
            return Flow::Normal;

        case NodeKind::Block: {
            scopes.emplace_back();
            Flow flow = Flow::Normal;
            for (Node* k : s->kids) {
                flow = execute(k);
                if (flow != Flow::Normal) break;
            }
            scopes.pop_back();
            return flow;
        }

        case NodeKind::If:
            if (evaluate(s->kids[0]).asBool()) return execute(s->kids[1]);
            if (s->kids.size() > 2) return execute(s->kids[2]);
            return Flow::Normal;

        case NodeKind::While:
            while (evaluate(s->kids[0]).asBool()) {
                Flow flow = execute(s->kids[1]);
                if (flow == Flow::Break) break;
                if (flow == Flow::Return) return flow;
            }
            return Flow::Normal;

        case NodeKind::For: {
            scopes.emplace_back();
            Flow result = Flow::Normal;
            execute(s->kids[0]);
            while (true) {
                if (s->kids[1]->kind != NodeKind::Empty && !evaluate(s->kids[1]).asBool()) break;
                Flow flow = execute(s->kids[3]);
                if (flow == Flow::Break) break;
                if (flow == Flow::Return) { result = flow; break; }
                if (s->kids[2]->kind != NodeKind::Empty) evaluate(s->kids[2]);
            }
            scopes.pop_back();
            return result;
        }

        case NodeKind::Return:
            returnValue = s->kids.empty() ? Value() : evaluate(s->kids[0]);
            return Flow::Return;

        case NodeKind::Break:
            return Flow::Break;

        case NodeKind::Continue:
            return Flow::Continue;

        case NodeKind::Cout: {
            std::vector<Value> parts;
            for (Node* k : s->kids) {
                if (k->kind == NodeKind::Endl) parts.push_back(Value::ofString("\n"));
                else parts.push_back(evaluate(k));
            }
            for (const Value& v : parts) print(v);
            std::cout.flush();
            return Flow::Normal;
        }

        case NodeKind::Cin:
            std::cout.flush();
            for (Node* k : s->kids) read(variable(k->name, k->line));
            return Flow::Normal;

        default:
            return Flow::Normal;
    }
}

void Interpreter::read(Value& target) {
    if (target.t == Value::String) {
        std::cin >> target.s;
    } else if (target.t == Value::Double) {
        std::cin >> target.d;
    } else {
        long long x = 0;
        std::cin >> x;
        target.i = target.t == Value::Bool ? (x != 0) : x;
    }
}

void Interpreter::print(const Value& v) {
    switch (v.t) {
        case Value::Int:
        case Value::Bool:   std::cout << v.i; break;
        case Value::Double: std::cout << v.d; break;
        case Value::String: std::cout << v.s; break;
        default: break;
    }
}

Value Interpreter::evaluate(Node* e) {
    switch (e->kind) {
        case NodeKind::IntLit:    return Value::ofInt(e->ival);
        case NodeKind::DoubleLit: return Value::ofDouble(e->dval);
        case NodeKind::StringLit: return Value::ofString(e->name);
        case NodeKind::BoolLit:   return Value::ofBool(e->ival != 0);

        case NodeKind::Ident:
            return variable(e->name, e->line);

        case NodeKind::Assign: {
            Value v = evaluate(e->kids[1]);
            Value& target = variable(e->kids[0]->name, e->line);
            if (e->name != "=") v = binary(e->name.substr(0, 1), target, v, e->line);
            target = convert(v, typeName(target.t));
            return target;
        }

        case NodeKind::Binary: {
            const std::string& op = e->name;
            if (op == "&&") {
                if (!evaluate(e->kids[0]).asBool()) return Value::ofBool(false);
                return Value::ofBool(evaluate(e->kids[1]).asBool());
            }
            if (op == "||") {
                if (evaluate(e->kids[0]).asBool()) return Value::ofBool(true);
                return Value::ofBool(evaluate(e->kids[1]).asBool());
            }
            Value a = evaluate(e->kids[0]);
            Value b = evaluate(e->kids[1]);
            return binary(op, a, b, e->line);
        }

        case NodeKind::Unary: {
            Value v = evaluate(e->kids[0]);
            if (e->name == "!") return Value::ofBool(!v.asBool());
            if (e->name == "-") return v.t == Value::Double ? Value::ofDouble(-v.d) : Value::ofInt(-v.asInt());
            return v.t == Value::Double ? v : Value::ofInt(v.asInt());
        }

        case NodeKind::IncDec: {
            Value& target = variable(e->kids[0]->name, e->line);
            Value old = target;
            Value changed = binary(e->name == "++" ? "+" : "-", target, Value::ofInt(1), e->line);
            target = convert(changed, typeName(target.t));
            return e->prefix ? target : old;
        }

        case NodeKind::Call: {
            std::vector<Value> args;
            for (Node* arg : e->kids) args.push_back(evaluate(arg));
            return invoke(funcs.at(e->name), args, e->line);
        }

        default:
            throw RuntimeError(e->line, "невідомий вираз");
    }
}

template <typename T>
bool compare(const std::string& op, const T& x, const T& y, bool& result) {
    if (op == "==")      result = x == y;
    else if (op == "!=") result = x != y;
    else if (op == "<")  result = x < y;
    else if (op == ">")  result = x > y;
    else if (op == "<=") result = x <= y;
    else if (op == ">=") result = x >= y;
    else return false;
    return true;
}

Value Interpreter::binary(const std::string& op, const Value& a, const Value& b, int line) {
    bool cmp = false;

    if (a.t == Value::String || b.t == Value::String) {
        if (op == "+") return Value::ofString(a.s + b.s);
        if (compare(op, a.s, b.s, cmp)) return Value::ofBool(cmp);
        throw RuntimeError(line, "оператор " + op + " не застосовний до string");
    }

    if (a.t == Value::Double || b.t == Value::Double) {
        double x = a.asDouble(), y = b.asDouble();
        if (op == "+") return Value::ofDouble(x + y);
        if (op == "-") return Value::ofDouble(x - y);
        if (op == "*") return Value::ofDouble(x * y);
        if (op == "/") return Value::ofDouble(x / y);
        if (compare(op, x, y, cmp)) return Value::ofBool(cmp);
        throw RuntimeError(line, "оператор " + op + " не застосовний до double");
    }

    long long x = a.asInt(), y = b.asInt();
    if (op == "+") return Value::ofInt(x + y);
    if (op == "-") return Value::ofInt(x - y);
    if (op == "*") return Value::ofInt(x * y);
    if (op == "/" || op == "%") {
        if (y == 0) throw RuntimeError(line, "ділення на нуль");
        return Value::ofInt(op == "/" ? x / y : x % y);
    }
    if (compare(op, x, y, cmp)) return Value::ofBool(cmp);
    throw RuntimeError(line, "невідомий оператор " + op);
}

}

int runProgram(Node* program) {
    Interpreter interpreter;
    return interpreter.run(program);
}
