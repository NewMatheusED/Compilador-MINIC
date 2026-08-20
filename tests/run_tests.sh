set -u

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

C_BIN="bin/minic_lexer"
PY_CLI="src/lexer/cli.py"
TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT

PASS=0
FAIL=0

if [ ! -x "$C_BIN" ]; then
    echo "compilando lexer em C..."
    mkdir -p bin
    gcc -Wall -Wextra -std=c99 -O2 -o "$C_BIN" src/lexer/lexer.c src/lexer/main.c || exit 1
fi

echo "=== 1) testes de oraculo ==="

gcc -Wall -Wextra -std=c99 -O2 -o "$TMP_DIR/oraculo_c" \
    src/lexer/lexer.c tests/oraculo/test_lexer_oracle.c || exit 1

if "$TMP_DIR/oraculo_c" > "$TMP_DIR/oraculo_c.log" 2>&1; then
    echo "PASSOU [oraculo/C]"
    PASS=$((PASS + 1))
else
    echo "FALHOU [oraculo/C]"
    cat "$TMP_DIR/oraculo_c.log"
    FAIL=$((FAIL + 1))
fi

if python3 tests/oraculo/test_lexer_oracle.py > "$TMP_DIR/oraculo_py.log" 2>&1; then
    echo "PASSOU [oraculo/Python]"
    PASS=$((PASS + 1))
else
    echo "FALHOU [oraculo/Python]"
    cat "$TMP_DIR/oraculo_py.log"
    FAIL=$((FAIL + 1))
fi

echo ""
echo "=== 2) regressao e paridade C vs Python (examples/ vs tests/results/) ==="

run_case() {
    local mc_file="$1"
    local category="$2"     # validos | invalidos
    local expected_exit="$3"

    local name
    name=$(basename "$mc_file" .mc)
    local golden="tests/results/${category}/${name}.tokens.txt"

    if [ ! -f "$golden" ]; then
        echo "SEM RESULTADO DE REFERENCIA: $golden"
        FAIL=$((FAIL + 1))
        return
    fi

    "$C_BIN" --tokens "$mc_file" > "$TMP_DIR/c_out.txt" 2>/dev/null
    local c_exit=$?

    python3 "$PY_CLI" --tokens "$mc_file" > "$TMP_DIR/py_out.txt" 2>/dev/null
    local py_exit=$?

    local ok=1

    if [ "$c_exit" -ne "$expected_exit" ]; then
        echo "FALHOU [$category/$name] codigo de saida do C: esperado $expected_exit, obtido $c_exit"
        ok=0
    fi
    if [ "$py_exit" -ne "$expected_exit" ]; then
        echo "FALHOU [$category/$name] codigo de saida do Python: esperado $expected_exit, obtido $py_exit"
        ok=0
    fi

    if ! diff -u "$golden" "$TMP_DIR/c_out.txt" > "$TMP_DIR/diff_c.txt"; then
        echo "FALHOU [$category/$name] saida do C difere do resultado de referencia:"
        cat "$TMP_DIR/diff_c.txt"
        ok=0
    fi
    if ! diff -u "$golden" "$TMP_DIR/py_out.txt" > "$TMP_DIR/diff_py.txt"; then
        echo "FALHOU [$category/$name] saida do Python difere do resultado de referencia:"
        cat "$TMP_DIR/diff_py.txt"
        ok=0
    fi

    if [ "$ok" -eq 1 ]; then
        echo "PASSOU [$category/$name]"
        PASS=$((PASS + 1))
    else
        FAIL=$((FAIL + 1))
    fi
}

for f in examples/validos/*.mc; do
    run_case "$f" "validos" 0
done

for f in examples/invalidos/*.mc; do
    run_case "$f" "invalidos" 2
done

echo ""
echo "resultado: $PASS passaram, $FAIL falharam"

if [ "$FAIL" -ne 0 ]; then
    exit 1
fi
exit 0
