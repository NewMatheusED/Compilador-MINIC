#include "src/lexer/lexer.c"

#include <stdio.h>
#include <stdlib.h>

static void json_print_escaped(const char *s) {
    putchar('"');
    for (const unsigned char *p = (const unsigned char *) s; *p; p++) {
        switch (*p) {
            case '"':  fputs("\\\"", stdout); break;
            case '\\': fputs("\\\\", stdout); break;
            case '\n': fputs("\\n", stdout); break;
            case '\r': fputs("\\r", stdout); break;
            case '\t': fputs("\\t", stdout); break;
            default:
                if (*p < 0x20) {
                    printf("\\u%04x", *p);
                } else {
                    putchar(*p);
                }
        }
    }
    putchar('"');
}

static void print_token_json(const Token *tok) {
    printf("{\"token\": ");
    json_print_escaped(token_type_name(tok->type));
    printf(", \"lexeme\": ");
    json_print_escaped(tok->lexeme);
    printf(", \"line\": %d, \"column\": %d}\n", tok->line, tok->col);
}

static void erro_json(const char *msg, int line, int col) {
    printf("{\"error\": \"lexico\", \"message\": ");
    json_print_escaped(msg);
    printf(", \"line\": %d, \"column\": %d}\n", line, col);
}

static char *ler_arquivo(const char *caminho) {
    FILE *f = fopen(caminho, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long tamanho = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (tamanho < 0) { fclose(f); return NULL; }

    char *buf = (char *) malloc((size_t) tamanho + 1);
    if (!buf) { fclose(f); return NULL; }

    size_t lidos = fread(buf, 1, (size_t) tamanho, f);
    buf[lidos] = '\0';
    fclose(f);
    return buf;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "uso: scanner arquivo.c|arquivo.minic\n");
        return 1;
    }

    char *fonte = ler_arquivo(argv[1]);
    if (!fonte) {
        fprintf(stderr, "erro: nao foi possivel abrir o arquivo \"%s\"\n", argv[1]);
        return 1;
    }

    Lexer lx;
    lexer_init(&lx, fonte);
    lexer_set_error_sink(&lx, erro_json);

    for (;;) {
        Token tok = lexer_next_token(&lx);
        print_token_json(&tok);
        int fim = (tok.type == TOK_EOF);
        token_free(&tok);
        if (fim) break;
    }

    int erros = lx.error_count;
    free(fonte);
    return erros > 0 ? 2 : 0;
}
