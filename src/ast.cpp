#include "ast.hpp"

#include <cstdio>
#include <iostream>

Node* astRoot = nullptr;

static const char* KIND_NAMES[] = {
    "Program", "Using", "Function", "Params", "Param", "DeclStmt", "VarDecl", "Block",
    "If", "While", "For", "Return", "Break", "Continue", "ExprStmt", "Empty", "Cout", "Cin", "Endl",
    "Assign", "Binary", "Unary", "IncDec", "Call", "Args",
    "IntLit", "DoubleLit", "StringLit", "BoolLit", "Ident"
};

const char* kindName(NodeKind kind) {
    return KIND_NAMES[(int)kind];
}

std::string visible(const std::string& s) {
    std::string r;
    for (char c : s) {
        if (c == '\n')      r += "\\n";
        else if (c == '\t') r += "\\t";
        else                r += c;
    }
    return r;
}

static std::string formatDouble(double d) {
    char buf[32];
    snprintf(buf, sizeof buf, "%g", d);
    std::string s = buf;
    if (s.find_first_of(".eEn") == std::string::npos) s += ".0";
    return s;
}

std::string nodeLabel(const Node* n) {
    switch (n->kind) {
        case NodeKind::Using:     return "namespace " + n->name;
        case NodeKind::Function:  return n->type + " " + n->name + "()";
        case NodeKind::Param:
        case NodeKind::VarDecl:   return n->type + " " + n->name;
        case NodeKind::DeclStmt:  return n->type;
        case NodeKind::Assign:
        case NodeKind::Binary:
        case NodeKind::Unary:
        case NodeKind::Ident:     return n->name;
        case NodeKind::IncDec:    return n->name + (n->prefix ? " prefix" : " postfix");
        case NodeKind::Call:      return n->name + "()";
        case NodeKind::IntLit:    return std::to_string(n->ival);
        case NodeKind::DoubleLit: return formatDouble(n->dval);
        case NodeKind::StringLit: return "\"" + visible(n->name) + "\"";
        case NodeKind::BoolLit:   return n->ival ? "true" : "false";
        default:                  return "";
    }
}

static void printNode(const Node* n, const std::string& indent, bool last, bool root) {
    std::string label = kindName(n->kind);
    std::string extra = nodeLabel(n);
    if (!extra.empty()) label += " " + extra;

    if (root) std::cout << label << "\n";
    else      std::cout << indent << (last ? "└── " : "├── ") << label << "\n";

    std::string childIndent = root ? "" : indent + (last ? "    " : "│   ");
    for (size_t i = 0; i < n->kids.size(); ++i)
        printNode(n->kids[i], childIndent, i + 1 == n->kids.size(), false);
}

void printTree(const Node* n) {
    printNode(n, "", true, true);
}

std::string escapeJson(const std::string& s) {
    std::string r;
    for (unsigned char c : s) {
        if (c == '"')       r += "\\\"";
        else if (c == '\\') r += "\\\\";
        else if (c == '\n') r += "\\n";
        else if (c == '\t') r += "\\t";
        else if (c < 0x20) {
            char buf[8];
            snprintf(buf, sizeof buf, "\\u%04x", c);
            r += buf;
        } else {
            r += (char)c;
        }
    }
    return r;
}

std::string toJson(const Node* n) {
    std::string out = "{\"kind\":\"" + std::string(kindName(n->kind)) + "\"" +
                      ",\"label\":\"" + escapeJson(nodeLabel(n)) + "\"" +
                      ",\"line\":" + std::to_string(n->line);
    if (!n->kids.empty()) {
        out += ",\"children\":[";
        for (size_t i = 0; i < n->kids.size(); ++i) {
            if (i) out += ",";
            out += toJson(n->kids[i]);
        }
        out += "]";
    }
    return out + "}";
}
