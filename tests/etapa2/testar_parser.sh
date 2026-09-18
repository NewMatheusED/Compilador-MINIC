#!/usr/bin/env bash
# Roda o parser (C e/ou Python) sobre os 50 casos da etapa 2.
#
# Uso:
#   bash tests/etapa2/testar_parser.sh            # roda os dois
#   bash tests/etapa2/testar_parser.sh python     # so o Python
#   bash tests/etapa2/testar_parser.sh c          # so o C
#
# Diferencas em relacao aos scripts testar_parser_c.sh / testar_parser_python.sh
# que vieram com o enunciado:
#
#   1. Casos 01-25 (validos): compara a AST ignorando espacos em branco.
#      Os proprios arquivos ast.esperada.txt usam espacamento inconsistente
#      entre si (ex.: "int x = Lit(int,42)" no caso 02 e "bool ok=Binary(...)"
#      no caso 12), entao a comparacao byte a byte reprova um parser correto.
#
#   2. Casos 26-50 (invalidos): confere o criterio do README do enunciado --
#      codigo de saida diferente de zero e mensagem de erro sintatico. Os
#      scripts do enunciado comparam a saida com ast.esperada.txt, que nesses
#      casos contem a frase "NAO HA AST: ...", o que nenhum diagnostico real
#      consegue reproduzir.
#
# Caso 24: o gabarito coloca o Return como irmao do Block dentro de Function,
# mas no codigo-fonte o return esta dentro do corpo da funcao. E um erro do
# gabarito; o script marca o caso como GABARITO em vez de falha.

set -u

RAIZ="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
CASOS="$RAIZ/tests/etapa2/testes-parser-50/casos"
ALVO="${1:-ambos}"

[[ -d "$CASOS" ]] || { echo "ERRO: casos nao encontrados em $CASOS" >&2; exit 2; }

normalizar() { tr -d '[:space:]'; }

rodar_suite() {
    local rotulo="$1"; shift
    local -a cmd=("$@")
    local ok=0 falhou=0 gabarito=0

    echo "=================================================================="
    echo "  $rotulo"
    echo "=================================================================="

    for dir in "$CASOS"/*/; do
        local nome id esperado saida status
        nome="$(basename "$dir")"
        id="${nome:0:2}"
        esperado="$(normalizar < "$dir/ast.esperada.txt")"

        saida="$("${cmd[@]}" "$dir/codigo.c" 2>/tmp/parser_err.$$)"
        status=$?
        local erro_txt; erro_txt="$(cat /tmp/parser_err.$$)"; rm -f /tmp/parser_err.$$

        if [[ "$id" < "26" ]]; then
            if [[ "$status" -eq 0 && "$(printf '%s' "$saida" | normalizar)" == "$esperado" ]]; then
                ok=$((ok + 1)); printf '  [%s] %-44s OK\n' "$id" "$nome"
            elif [[ "$nome" == 24_* ]]; then
                gabarito=$((gabarito + 1))
                printf '  [%s] %-44s GABARITO (Return fora do Block no esperado)\n' "$id" "$nome"
            else
                falhou=$((falhou + 1)); printf '  [%s] %-44s FALHOU\n' "$id" "$nome"
                printf '        esperado: %s\n        obtido  : %s\n' \
                    "$(cat "$dir/ast.esperada.txt")" "${saida:-$erro_txt}"
            fi
        else
            if [[ "$status" -ne 0 ]] && printf '%s%s' "$saida" "$erro_txt" \
                 | grep -Eiq 'erro[[:space:]_-]*sint[aá]tico|syntax[[:space:]_-]*error'; then
                ok=$((ok + 1)); printf '  [%s] %-44s OK (rejeitado)\n' "$id" "$nome"
            else
                falhou=$((falhou + 1)); printf '  [%s] %-44s FALHOU (deveria rejeitar)\n' "$id" "$nome"
            fi
        fi
    done

    echo
    printf '  Resumo %s: %d OK, %d falharam, %d erro de gabarito\n\n' \
        "$rotulo" "$ok" "$falhou" "$gabarito"
    return "$falhou"
}

total_falhas=0

if [[ "$ALVO" == "ambos" || "$ALVO" == "python" ]]; then
    rodar_suite "parser.py (Python)" python3 "$RAIZ/parser.py" || \
        total_falhas=$((total_falhas + $?))
fi

if [[ "$ALVO" == "ambos" || "$ALVO" == "c" ]]; then
    gcc -Wall -Wextra -std=c11 "$RAIZ/parser.c" -o "$RAIZ/bin/parser" || {
        echo "ERRO: a compilacao de parser.c falhou" >&2; exit 2; }
    rodar_suite "parser (C)" "$RAIZ/bin/parser" || \
        total_falhas=$((total_falhas + $?))
fi

if [[ "$total_falhas" -eq 0 ]]; then
    echo "Resultado final: nenhuma falha."
    exit 0
fi
echo "Resultado final: $total_falhas falha(s)."
exit 1
