#pragma once

#include <string>
#include <vector>

enum class NodeKind {
    Program, Using, Function, Params, Param, DeclStmt, VarDecl, Block,
    If, While, For, Return, Break, Continue, ExprStmt, Empty, Cout, Cin, Endl,
    Assign, Binary, Unary, IncDec, Call, Args,
    IntLit, DoubleLit, StringLit, BoolLit, Ident
};

struct Node {
    NodeKind kind;
    int line = 0;
    std::string name;
    std::string type;
    long long ival = 0;
    double dval = 0;
    bool prefix = false;
    std::vector<Node*> kids;

    Node(NodeKind k, int l, const std::string& n = "") : kind(k), line(l), name(n) {}
    ~Node() { for (Node* c : kids) delete c; }
    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;

    Node* add(Node* child) {
        kids.push_back(child);
        return this;
    }
};

extern Node* astRoot;

inline Node* makeNode(NodeKind kind, int line, const std::string& name = "") {
    return new Node(kind, line, name);
}

const char* kindName(NodeKind kind);
std::string nodeLabel(const Node* n);
std::string visible(const std::string& s);
std::string escapeJson(const std::string& s);
void printTree(const Node* n);
std::string toJson(const Node* n);
