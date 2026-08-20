#include <stdio.h>
#include <string.h>
#include "../../src/lexer/lexer.h"

static int total = 0;
static int falhas = 0;

static void check(Lexer *lx, TokenType tipo_esperado, const char *lexema_esperado,
                   int linha_esperada, int col_esperada, const char *caso) {
    Token tok = lexer_next_token(lx);
    total++;

    int ok = (tok.type == tipo_esperado) &&
             (strcmp(tok.lexeme, lexema_esperado) == 0) &&
             (tok.line == linha_esperada) &&
             (tok.col == col_esperada);

    if (!ok) {
        falhas++;
        printf("FALHOU [%s]\n", caso);
        printf("  esperado: tipo=%s lexema=\"%s\" linha=%d col=%d\n",
               token_type_name(tipo_esperado), lexema_esperado, linha_esperada, col_esperada);
        printf("  obtido:   tipo=%s lexema=\"%s\" linha=%d col=%d\n",
               token_type_name(tok.type), tok.lexeme, tok.line, tok.col);
    }

    token_free(&tok);
}

static void teste_tokens_basicos_e_colunas(void) {
    const char *caso = "tokens_basicos_e_colunas";
    Lexer lx;
    lexer_init(&lx, "int x = 10;");

    check(&lx, TOK_KW_INT, "int", 1, 1, caso);
    check(&lx, TOK_ID, "x", 1, 5, caso);
    check(&lx, TOK_ASSIGN, "=", 1, 7, caso);
    check(&lx, TOK_INT_LIT, "10", 1, 9, caso);
    check(&lx, TOK_SEMI, ";", 1, 11, caso);
    check(&lx, TOK_EOF, "", 1, 12, caso);

    if (lx.error_count != 0) {
        falhas++;
        printf("FALHOU [%s]: esperava 0 erros, obteve %d\n", caso, lx.error_count);
    }
}

static void teste_operadores_dois_caracteres(void) {
    const char *caso = "operadores_dois_caracteres";
    Lexer lx;
    lexer_init(&lx, "a <= b < c");

    check(&lx, TOK_ID, "a", 1, 1, caso);
    check(&lx, TOK_LE, "<=", 1, 3, caso);
    check(&lx, TOK_ID, "b", 1, 6, caso);
    check(&lx, TOK_LT, "<", 1, 8, caso);
    check(&lx, TOK_ID, "c", 1, 10, caso);
    check(&lx, TOK_EOF, "", 1, 11, caso);

    if (lx.error_count != 0) {
        falhas++;
        printf("FALHOU [%s]: esperava 0 erros, obteve %d\n", caso, lx.error_count);
    }
}

static void teste_ponto_sem_digito_nao_forma_real(void) {
    const char *caso = "ponto_sem_digito_nao_forma_real";
    Lexer lx;
    lexer_init(&lx, "3.14 3. .5 42");

    check(&lx, TOK_FLOAT_LIT, "3.14", 1, 1, caso);
    check(&lx, TOK_INT_LIT, "3", 1, 6, caso);
    check(&lx, TOK_INT_LIT, "5", 1, 10, caso);
    check(&lx, TOK_INT_LIT, "42", 1, 12, caso);
    check(&lx, TOK_EOF, "", 1, 14, caso);

    if (lx.error_count != 2) {
        falhas++;
        printf("FALHOU [%s]: esperava 2 erros (os dois \".\" isolados), obteve %d\n",
               caso, lx.error_count);
    }
}

static void teste_literais_de_caractere(void) {
    /* 'a' valido | '\n' escape valido | '' vazio invalido | 'ab' invalido */
    const char *caso = "literais_de_caractere";
    Lexer lx;
    lexer_init(&lx, "'a' '\\n' '' 'ab'");

    check(&lx, TOK_CHAR_LIT, "'a'", 1, 1, caso);
    check(&lx, TOK_CHAR_LIT, "'\\n'", 1, 5, caso);
    check(&lx, TOK_CHAR_LIT, "''", 1, 10, caso);
    check(&lx, TOK_CHAR_LIT, "'ab'", 1, 13, caso);
    check(&lx, TOK_EOF, "", 1, 17, caso);

    if (lx.error_count != 2) {
        falhas++;
        printf("FALHOU [%s]: esperava 2 erros ('' e 'ab'), obteve %d\n", caso, lx.error_count);
    }
}

static void teste_palavra_reservada_vs_identificador(void) {
    const char *caso = "palavra_reservada_vs_identificador";
    Lexer lx;
    lexer_init(&lx, "int integer intx");

    check(&lx, TOK_KW_INT, "int", 1, 1, caso);
    check(&lx, TOK_ID, "integer", 1, 5, caso);
    check(&lx, TOK_ID, "intx", 1, 13, caso);
    check(&lx, TOK_EOF, "", 1, 17, caso);

    if (lx.error_count != 0) {
        falhas++;
        printf("FALHOU [%s]: esperava 0 erros, obteve %d\n", caso, lx.error_count);
    }
}

static void teste_comentario_de_linha_preserva_quebra(void) {
    const char *caso = "comentario_de_linha_preserva_quebra";
    Lexer lx;
    lexer_init(&lx, "a//comentario\nb");

    check(&lx, TOK_ID, "a", 1, 1, caso);
    check(&lx, TOK_ID, "b", 2, 1, caso);
    check(&lx, TOK_EOF, "", 2, 2, caso);

    if (lx.error_count != 0) {
        falhas++;
        printf("FALHOU [%s]: esperava 0 erros, obteve %d\n", caso, lx.error_count);
    }
}

int main(void) {
    teste_tokens_basicos_e_colunas();
    teste_operadores_dois_caracteres();
    teste_ponto_sem_digito_nao_forma_real();
    teste_literais_de_caractere();
    teste_palavra_reservada_vs_identificador();
    teste_comentario_de_linha_preserva_quebra();

    printf("\nresultado: %d verificacoes, %d falharam\n", total, falhas);
    return falhas == 0 ? 0 : 1;
}
