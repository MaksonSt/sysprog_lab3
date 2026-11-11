#!/usr/bin/env bash
set -u
cd "$(dirname "$0")/.."

BIN=./minicpp.exe
[ -x "$BIN" ] || BIN=./minicpp
OUT=build/test
mkdir -p "$OUT"

pass=0
fail=0
ok()  { echo "  OK    $1"; pass=$((pass + 1)); }
bad() { echo "  FAIL  $1"; fail=$((fail + 1)); }

run() { "$@" 2>&1 | tr -d '\r'; }

check() {
    if [ "$2" == "$3" ]; then ok "$1"; else bad "$1"; echo "$2" > "$OUT/a.txt"; echo "$3" > "$OUT/b.txt"; fi
}

echo "Інтерпретатор = JavaScript (node) = g++"
for name in factorial primes dangling_else types input; do
    src="examples/$name.cpp"
    input="examples/$name.in"
    [ -f "$input" ] || input=/dev/null

    "$BIN" "$src" --js "$OUT/$name.js" > /dev/null
    interp=$(run "$BIN" "$src" --run < "$input")
    node=$(run node "$OUT/$name.js" < "$input")
    check "$name: інтерпретатор = node" "$interp" "$node"

    if command -v g++ > /dev/null && g++ -std=c++17 -w -o "$OUT/$name.native.exe" "$src"; then
        native=$(run "$OUT/$name.native.exe" < "$input")
        check "$name: інтерпретатор = g++" "$interp" "$native"
    fi
done

echo
echo "Помилки"
expect_error() {
    local name=$1 file=$2 pattern=$3
    if ! "$BIN" "$file" > "$OUT/err.txt" 2>&1 && grep -q "$pattern" "$OUT/err.txt"; then
        ok "$name: $pattern"
    else
        bad "$name: очікувалось \"$pattern\""
    fi
}

printf 'int main() {\n    int x = 5;\n    int y = x @ 2;\n    return 0;\n}\n' > "$OUT/lex_error.cpp"
expect_error lex_error "$OUT/lex_error.cpp" "Лексична помилка (рядок 3)"
expect_error syntax_error   examples/syntax_error.cpp   "Знайдено синтаксичних помилок: 2"
expect_error semantic_error examples/semantic_error.cpp "Знайдено семантичних помилок: 6"

printf 'int main() {\n    int zero = 0;\n    return 5 / zero;\n}\n' > "$OUT/div_zero.cpp"
"$BIN" "$OUT/div_zero.cpp" --js "$OUT/div_zero.js" > /dev/null
interp=$(run "$BIN" "$OUT/div_zero.cpp" --run)
node=$(run node "$OUT/div_zero.js")
if [ "$interp" == "Помилка виконання (рядок 3): ділення на нуль" ] && [ "$interp" == "$node" ]; then
    ok "ділення на нуль: однакова помилка в інтерпретаторі і node"
else
    bad "ділення на нуль"
fi

echo
echo "Лексер і граматика"
if "$BIN" examples/factorial.cpp --tokens | grep -q "Усього токенів: 91"; then
    ok "factorial.cpp: 91 токен"
else
    bad "factorial.cpp: неправильна кількість токенів"
fi

if make -s conflict-demo 2>&1 | grep -q "1 shift/reduce conflict"; then
    ok "без %prec граматика має рівно 1 shift/reduce конфлікт (dangling else)"
else
    bad "conflict-demo не показав очікуваний конфлікт"
fi

echo
echo "Пройдено: $pass, не пройдено: $fail"
[ "$fail" -eq 0 ]
