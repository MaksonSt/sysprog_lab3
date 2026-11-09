#include "codegen_js.hpp"

#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

const char* RUNTIME = R"JS("use strict";

const $rt = (() => {
    const output = globalThis.$write || ((text) => process.stdout.write(text));
    let tokens = null;

    function next() {
        if (tokens === null) {
            const text = globalThis.$input !== undefined
                ? globalThis.$input
                : require("fs").readFileSync(0, "utf8");
            tokens = text.split(/\s+/).filter((t) => t.length > 0);
        }
        return tokens.length > 0 ? tokens.shift() : "";
    }

    function fail(message, line) {
        const error = new Error(message);
        error.line = line;
        throw error;
    }

    function fmt(x) {
        if (Number.isNaN(x)) return "nan";
        if (!Number.isFinite(x)) return x > 0 ? "inf" : "-inf";
        if (x === 0) return Object.is(x, -0) ? "-0" : "0";
        const [mantissa, exponent] = x.toExponential(5).split("e");
        const exp = Number(exponent);
        if (exp < -4 || exp >= 6) {
            const digits = String(Math.abs(exp)).padStart(2, "0");
            return mantissa.replace(/\.?0+$/, "") + "e" + (exp < 0 ? "-" : "+") + digits;
        }
        return String(Number(x.toPrecision(6)));
    }

    return {
        write: (...parts) => output(parts.join("")),
        fmt,
        idiv(a, b, line) {
            if (b === 0) fail("ділення на нуль", line);
            return Math.trunc(a / b);
        },
        imod(a, b, line) {
            if (b === 0) fail("ділення на нуль", line);
            return a % b;
        },
        readInt: () => Math.trunc(Number(next())) || 0,
        readDouble: () => Number(next()) || 0,
        readBool: () => Number(next()) !== 0,
        readString: () => next(),
        report(error) {
            const message = error.line !== undefined
                ? `Помилка виконання (рядок ${error.line}): ${error.message}`
                : `Помилка виконання: ${error.message}`;
            (globalThis.$error || console.error)(message);
            if (typeof process !== "undefined") process.exitCode = 1;
        },
    };
})();
)JS";

const std::set<std::string> JS_RESERVED = {
    "arguments", "await", "case", "catch", "class", "const", "debugger", "default", "delete",
    "do", "enum", "eval", "export", "extends", "finally", "function", "implements", "import",
    "in", "instanceof", "interface", "let", "new", "null", "package", "private", "protected",
    "public", "static", "super", "switch", "this", "throw", "try", "typeof", "undefined",
    "var", "with", "yield", "NaN", "Infinity", "Math", "Number", "String", "Boolean", "Object",
    "Error", "console", "process", "require", "globalThis"
};

struct Code {
    std::string text;
    int prec;
};

enum Prec {
    P_ASSIGN = 1, P_OR = 2, P_AND = 3, P_EQUALITY = 4, P_RELATION = 5,
    P_ADD = 6, P_MUL = 7, P_UNARY = 8, P_POSTFIX = 9, P_PRIMARY = 10
};

std::string jsName(const std::string& name) {
    return JS_RESERVED.count(name) ? name + "$" : name;
}

std::string jsString(const std::string& s) {
    std::string r = "\"";
    for (unsigned char c : s) {
        switch (c) {
            case '"':  r += "\\\""; break;
            case '\\': r += "\\\\"; break;
            case '\n': r += "\\n";  break;
            case '\t': r += "\\t";  break;
            case '\r': r += "\\r";  break;
            default:   r += (char)c;
        }
    }
    return r + "\"";
}

std::string jsNumber(double d) {
    char buf[40];
    for (int precision = 1; precision <= 17; ++precision) {
        snprintf(buf, sizeof buf, "%.*g", precision, d);
        if (strtod(buf, nullptr) == d) break;
    }
    return buf;
}

std::string wrap(const Code& c, int minPrec) {
    return c.prec < minPrec ? "(" + c.text + ")" : c.text;
}

int binaryPrec(const std::string& op) {
    if (op == "||") return P_OR;
    if (op == "&&") return P_AND;
    if (op == "==" || op == "!=") return P_EQUALITY;
    if (op == "<" || op == ">" || op == "<=" || op == ">=") return P_RELATION;
    if (op == "+" || op == "-") return P_ADD;
    return P_MUL;
}

bool isInteger(const std::string& t) {
    return t == "int" || t == "bool";
}

class Generator {
public:
    std::string run(Node* program);

private:
    std::ostringstream out;
    int indent = 0;
    std::vector<std::map<std::string, std::string>> scopes;
    std::map<std::string, Node*> funcs;
    std::string currentReturn;

    void line(const std::string& text);
    void declare(const std::string& name, const std::string& type) { scopes.back()[name] = type; }
    std::string lookup(const std::string& name) const;

    void function(Node* f);
    void statement(Node* s);
    void body(Node* s);
    std::string declaration(Node* d);
    std::string forInit(Node* s);

    std::string typeOf(Node* e);
    Code expression(Node* e);
    Code convert(const Code& c, const std::string& from, const std::string& to);
    Code arithmetic(const std::string& op, const Code& a, const std::string& ta,
                    const Code& b, const std::string& tb, int lineNo);
    Code toBool(Node* e);
    std::string printable(Node* e);
};

void Generator::line(const std::string& text) {
    out << std::string(indent * 4, ' ') << text << "\n";
}

std::string Generator::lookup(const std::string& name) const {
    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) return found->second;
    }
    return "int";
}

std::string Generator::run(Node* program) {
    out << RUNTIME << "\n";
    scopes.emplace_back();

    for (Node* item : program->kids)
        if (item->kind == NodeKind::Function) funcs[item->name] = item;

    bool first = true;
    for (Node* item : program->kids) {
        if (item->kind == NodeKind::Using) continue;
        if (item->kind == NodeKind::Function) {
            if (!first) out << "\n";
            function(item);
        } else if (item->kind == NodeKind::DeclStmt) {
            line(declaration(item) + ";");
        }
        first = false;
    }

    out << "\n";
    line("try {");
    line("    const code = main();");
    line("    if (typeof process !== \"undefined\") process.exitCode = code;");
    line("} catch (error) {");
    line("    $rt.report(error);");
    line("}");
    return out.str();
}

void Generator::function(Node* f) {
    currentReturn = f->type;
    scopes.emplace_back();
    std::string params;
    for (Node* p : f->kids[0]->kids) {
        if (!params.empty()) params += ", ";
        params += jsName(p->name);
        declare(p->name, p->type);
    }
    line("function " + jsName(f->name) + "(" + params + ") {");
    ++indent;
    for (Node* s : f->kids[1]->kids) statement(s);
    --indent;
    line("}");
    scopes.pop_back();
}

std::string Generator::declaration(Node* d) {
    std::string text = "let ";
    for (size_t i = 0; i < d->kids.size(); ++i) {
        Node* var = d->kids[i];
        std::string init;
        if (var->kids.empty()) {
            if (var->type == "string") init = "\"\"";
            else if (var->type == "bool") init = "false";
            else init = "0";
        } else {
            Node* e = var->kids[0];
            init = wrap(convert(expression(e), typeOf(e), var->type), P_ASSIGN);
        }
        declare(var->name, var->type);
        if (i) text += ", ";
        text += jsName(var->name) + " = " + init;
    }
    return text;
}

void Generator::body(Node* s) {
    ++indent;
    if (s->kind == NodeKind::Block) {
        scopes.emplace_back();
        for (Node* k : s->kids) statement(k);
        scopes.pop_back();
    } else {
        scopes.emplace_back();
        statement(s);
        scopes.pop_back();
    }
    --indent;
}

std::string Generator::forInit(Node* s) {
    if (s->kind == NodeKind::DeclStmt) return declaration(s);
    if (s->kind == NodeKind::ExprStmt) return expression(s->kids[0]).text;
    return "";
}

void Generator::statement(Node* s) {
    switch (s->kind) {
        case NodeKind::DeclStmt:
            line(declaration(s) + ";");
            break;

        case NodeKind::ExprStmt:
            line(expression(s->kids[0]).text + ";");
            break;

        case NodeKind::Block:
            line("{");
            body(s);
            line("}");
            break;

        case NodeKind::If: {
            line("if (" + expression(s->kids[0]).text + ") {");
            body(s->kids[1]);
            Node* rest = s->kids.size() > 2 ? s->kids[2] : nullptr;
            while (rest && rest->kind == NodeKind::If) {
                line("} else if (" + expression(rest->kids[0]).text + ") {");
                body(rest->kids[1]);
                rest = rest->kids.size() > 2 ? rest->kids[2] : nullptr;
            }
            if (rest) {
                line("} else {");
                body(rest);
            }
            line("}");
            break;
        }

        case NodeKind::While:
            line("while (" + expression(s->kids[0]).text + ") {");
            body(s->kids[1]);
            line("}");
            break;

        case NodeKind::For: {
            scopes.emplace_back();
            std::string init = forInit(s->kids[0]);
            std::string cond = s->kids[1]->kind == NodeKind::Empty ? "" : expression(s->kids[1]).text;
            std::string step = s->kids[2]->kind == NodeKind::Empty ? "" : expression(s->kids[2]).text;
            line("for (" + init + "; " + cond + "; " + step + ") {");
            body(s->kids[3]);
            line("}");
            scopes.pop_back();
            break;
        }

        case NodeKind::Return:
            if (s->kids.empty()) {
                line("return;");
            } else {
                Node* e = s->kids[0];
                line("return " + convert(expression(e), typeOf(e), currentReturn).text + ";");
            }
            break;

        case NodeKind::Break:
            line("break;");
            break;

        case NodeKind::Continue:
            line("continue;");
            break;

        case NodeKind::Cout: {
            std::string parts;
            for (Node* k : s->kids) {
                if (!parts.empty()) parts += ", ";
                parts += printable(k);
            }
            line("$rt.write(" + parts + ");");
            break;
        }

        case NodeKind::Cin:
            for (Node* k : s->kids) {
                std::string t = lookup(k->name);
                std::string reader = t == "string" ? "readString" : t == "double" ? "readDouble"
                                   : t == "bool" ? "readBool" : "readInt";
                line(jsName(k->name) + " = $rt." + reader + "();");
            }
            break;

        default:
            break;
    }
}

std::string Generator::printable(Node* e) {
    if (e->kind == NodeKind::Endl) return "\"\\n\"";
    std::string t = typeOf(e);
    Code c = expression(e);
    if (t == "double") return "$rt.fmt(" + c.text + ")";
    if (t == "bool") return "Number(" + c.text + ")";
    return wrap(c, P_ASSIGN);
}

std::string Generator::typeOf(Node* e) {
    switch (e->kind) {
        case NodeKind::IntLit:    return "int";
        case NodeKind::DoubleLit: return "double";
        case NodeKind::StringLit: return "string";
        case NodeKind::BoolLit:   return "bool";
        case NodeKind::Ident:     return lookup(e->name);
        case NodeKind::Assign:
        case NodeKind::IncDec:    return lookup(e->kids[0]->name);
        case NodeKind::Call:      return funcs.count(e->name) ? funcs[e->name]->type : "int";
        case NodeKind::Unary: {
            if (e->name == "!") return "bool";
            return typeOf(e->kids[0]) == "double" ? "double" : "int";
        }
        case NodeKind::Binary: {
            const std::string& op = e->name;
            int p = binaryPrec(op);
            if (p <= P_RELATION) return "bool";
            std::string a = typeOf(e->kids[0]);
            std::string b = typeOf(e->kids[1]);
            if (a == "string" || b == "string") return "string";
            return (a == "double" || b == "double") ? "double" : "int";
        }
        default:
            return "int";
    }
}

Code Generator::convert(const Code& c, const std::string& from, const std::string& to) {
    if (from == to) return c;
    if (to == "int") {
        if (from == "double") return {"Math.trunc(" + c.text + ")", P_PRIMARY};
        if (from == "bool") return {"Number(" + c.text + ")", P_PRIMARY};
    }
    if (to == "double" && from == "bool") return {"Number(" + c.text + ")", P_PRIMARY};
    if (to == "bool" && (from == "int" || from == "double")) return {wrap(c, P_RELATION) + " !== 0", P_EQUALITY};
    return c;
}

Code Generator::toBool(Node* e) {
    return convert(expression(e), typeOf(e), "bool");
}

Code Generator::arithmetic(const std::string& op, const Code& a, const std::string& ta,
                           const Code& b, const std::string& tb, int lineNo) {
    if (isInteger(ta) && isInteger(tb) && (op == "/" || op == "%")) {
        std::string fn = op == "/" ? "$rt.idiv(" : "$rt.imod(";
        return {fn + a.text + ", " + b.text + ", " + std::to_string(lineNo) + ")", P_PRIMARY};
    }
    int p = binaryPrec(op);
    return {wrap(a, p) + " " + op + " " + wrap(b, p + 1), p};
}

Code Generator::expression(Node* e) {
    switch (e->kind) {
        case NodeKind::IntLit:    return {std::to_string(e->ival), P_PRIMARY};
        case NodeKind::DoubleLit: return {jsNumber(e->dval), P_PRIMARY};
        case NodeKind::StringLit: return {jsString(e->name), P_PRIMARY};
        case NodeKind::BoolLit:   return {e->ival ? "true" : "false", P_PRIMARY};
        case NodeKind::Ident:     return {jsName(e->name), P_PRIMARY};

        case NodeKind::Assign: {
            Node* target = e->kids[0];
            Node* value = e->kids[1];
            std::string tt = lookup(target->name);
            std::string tv = typeOf(value);
            std::string name = jsName(target->name);
            Code rhs = expression(value);

            if (e->name == "=")
                return {name + " = " + wrap(convert(rhs, tv, tt), P_ASSIGN), P_ASSIGN};

            std::string op = e->name.substr(0, 1);
            bool needsExpand = (isInteger(tt) && (op == "/" || tv == "double")) || tt == "bool";
            if (!needsExpand)
                return {name + " " + e->name + " " + wrap(rhs, P_ASSIGN), P_ASSIGN};

            std::string resultType = (tt == "double" || tv == "double") ? "double" : "int";
            Code combined = arithmetic(op, {name, P_PRIMARY}, tt, rhs, tv, e->line);
            return {name + " = " + wrap(convert(combined, resultType, tt), P_ASSIGN), P_ASSIGN};
        }

        case NodeKind::Binary: {
            const std::string& op = e->name;
            Node* left = e->kids[0];
            Node* right = e->kids[1];

            if (op == "&&" || op == "||") {
                int p = binaryPrec(op);
                return {wrap(toBool(left), p) + " " + op + " " + wrap(toBool(right), p + 1), p};
            }

            std::string ta = typeOf(left);
            std::string tb = typeOf(right);
            Code a = expression(left);
            Code b = expression(right);
            int p = binaryPrec(op);

            if (p <= P_RELATION) {
                if (ta == "bool" && tb != "bool") a = {"Number(" + a.text + ")", P_PRIMARY};
                if (tb == "bool" && ta != "bool") b = {"Number(" + b.text + ")", P_PRIMARY};
                std::string jsOp = op == "==" ? "===" : op == "!=" ? "!==" : op;
                return {wrap(a, p) + " " + jsOp + " " + wrap(b, p + 1), p};
            }
            return arithmetic(op, a, ta, b, tb, e->line);
        }

        case NodeKind::Unary: {
            Code operand = expression(e->kids[0]);
            std::string text = wrap(operand, P_UNARY);
            if (!text.empty() && (text[0] == '-' || text[0] == '+')) text = " " + text;
            if (e->name == "+" && typeOf(e->kids[0]) == "bool") return {"Number(" + operand.text + ")", P_PRIMARY};
            return {e->name + text, P_UNARY};
        }

        case NodeKind::IncDec: {
            std::string name = jsName(e->kids[0]->name);
            if (e->prefix) return {e->name + name, P_UNARY};
            return {name + e->name, P_POSTFIX};
        }

        case NodeKind::Call: {
            Node* f = funcs.count(e->name) ? funcs[e->name] : nullptr;
            std::string args;
            for (size_t i = 0; i < e->kids.size(); ++i) {
                Node* arg = e->kids[i];
                Code c = expression(arg);
                if (f && i < f->kids[0]->kids.size())
                    c = convert(c, typeOf(arg), f->kids[0]->kids[i]->type);
                if (i) args += ", ";
                args += wrap(c, P_ASSIGN);
            }
            return {jsName(e->name) + "(" + args + ")", P_PRIMARY};
        }

        default:
            return {"undefined", P_PRIMARY};
    }
}

}

std::string generateJs(Node* program) {
    Generator generator;
    return generator.run(program);
}
