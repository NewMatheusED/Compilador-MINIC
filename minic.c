#include "src/lexer/lexer.c"
#include "src/parser/ast.c"
#include "src/parser/parser.c"
#include "src/semantic/semantic.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

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
#ifdef _WIN32
    /* senao o Windows troca \n por \r\n */
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    if (argc < 2) {
        fprintf(stderr, "uso: minic arquivo.c\n");
        return 1;
    }

    char *fonte = ler_arquivo(argv[1]);
    if (!fonte) {
        fprintf(stderr, "erro: nao foi possivel abrir o arquivo \"%s\"\n", argv[1]);
        return 1;
    }

    ErroSintatico erro_sint;
    memset(&erro_sint, 0, sizeof(erro_sint));

    AstNode *arvore = parse_source(fonte, &erro_sint);
    free(fonte);

    if (!arvore) {
        if (erro_sint.linha > 0) {
            fprintf(stderr, "Erro sintatico na linha %d, coluna %d: %s.\n",
                    erro_sint.linha, erro_sint.coluna, erro_sint.mensagem);
        } else {
            fprintf(stderr, "Erro sintatico: %s.\n", erro_sint.mensagem);
        }
        return 1;
    }

    char *saida = NULL;
    int erros = semantic_analisar(arvore, &saida);
    fwrite(saida, 1, strlen(saida), stdout);
    fflush(stdout);

    free(saida);
    ast_free(arvore);
    return erros > 0 ? 3 : 0;
}
