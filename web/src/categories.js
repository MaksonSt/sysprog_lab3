// Категорії вузлів AST і їхні кольори (легенда під деревом)
export const CATEGORIES = [
    { name: "Структура", color: "#7b61d9", kinds: ["Program", "Using", "Function", "Params", "Param"] },
    { name: "Оголошення", color: "#2b9a66", kinds: ["DeclStmt", "VarDecl"] },
    { name: "Оператори", color: "#2f6fde", kinds: ["Block", "If", "While", "For", "Return", "Break", "Continue", "ExprStmt", "Empty"] },
    { name: "Ввід / вивід", color: "#d9822b", kinds: ["Cout", "Cin", "Endl"] },
    { name: "Вирази", color: "#d6455d", kinds: ["Assign", "Binary", "Unary", "IncDec", "Call", "Args"] },
    { name: "Значення", color: "#6b7785", kinds: ["IntLit", "DoubleLit", "StringLit", "BoolLit", "Ident"] },
];

const COLOR_BY_KIND = new Map(CATEGORIES.flatMap((c) => c.kinds.map((kind) => [kind, c.color])));

export function colorOf(kind) {
    return COLOR_BY_KIND.get(kind) || "#6b7785";
}
