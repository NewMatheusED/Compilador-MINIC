#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"

/* Driver de linha de comando para a etapa 1 (analisador lexico) do MINIC.
 *
 * Uso:
 *   minic_lexer [--tokens] arquivo.mc
 *
 * --tokens exibe cada token reconhecido no formato:
 *   linha:coluna  TIPO  "lexema"
 *
 * Erros lexicos sao impressos no formato definido pela especificacao
 * (secao 12) assim que ocorrem. Codigos de saida seguem a secao 11.1:
 *   0 sucesso, 1 uso invalido, 2 erro lexico.
 */

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = (char *) malloc((size_t) size + 1);
    size_t read = fread(buf, 1, (size_t) size, f);
    buf[read] = '\0';
    fclose(f);
    return buf;
}

int main(int argc, char **argv) {
    int show_tokens = 0;
    const char *path = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--tokens") == 0) {
            show_tokens = 1;
        } else {
            path = argv[i];
        }
    }

    if (path == NULL) {
        fprintf(stderr, "uso: minic_lexer [--tokens] arquivo.mc\n");
        return 1;
    }

    char *source = read_file(path);
    if (source == NULL) {
        fprintf(stderr, "erro: nao foi possivel abrir o arquivo \"%s\"\n", path);
        return 1;
    }

    Lexer lx;
    lexer_init(&lx, source);

    int token_count = 0;
    for (;;) {
        Token tok = lexer_next_token(&lx);
        token_count++;
        if (show_tokens) {
            printf("%d:%d\t%s\t\"%s\"\n", tok.line, tok.col, token_type_name(tok.type), tok.lexeme);
        }
        int is_eof = (tok.type == TOK_EOF);
        token_free(&tok);
        if (is_eof) break;
    }

    if (show_tokens) {
        fprintf(stderr, "total de tokens: %d\n", token_count);
        fprintf(stderr, "total de erros lexicos: %d\n", lx.error_count);
    }

    free(source);

    return (lx.error_count > 0) ? 2 : 0;
}
