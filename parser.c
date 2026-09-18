#include "src/lexer/lexer.c"
#include "src/parser/ast.c"
#include "src/parser/parser.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FRASE_SEM_AST "N\xc3\x83O H\xc3\x81 AST: o parser deve rejeitar a entrada."

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
        fprintf(stderr, "uso: parser arquivo.c\n");
        return 1;
    }

    char *fonte = ler_arquivo(argv[1]);
    if (!fonte) {
        fprintf(stderr, "erro: nao foi possivel abrir o arquivo \"%s\"\n", argv[1]);
        return 1;
    }

    ErroSintatico erro;
    memset(&erro, 0, sizeof(erro));

    AstNode *arvore = parse_source(fonte, &erro);
    free(fonte);

    if (!arvore) {
        const char *modo = getenv("MODO_SCRIPT");
        if (modo && strcmp(modo, "1") == 0) {
            printf("%s\n", FRASE_SEM_AST);
        } else if (erro.linha > 0) {
            fprintf(stderr, "Erro sintatico na linha %d, coluna %d: %s.\n",
                    erro.linha, erro.coluna, erro.mensagem);
        } else {
            fprintf(stderr, "Erro sintatico: %s.\n", erro.mensagem);
        }
        return 1;
    }

    ast_print(arvore);
    putchar('\n');
    ast_free(arvore);
    return 0;
}
