#include "src/ir/ir.c"
#include "src/lexer/lexer.c"
#include "src/optimizer/opt.c"
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

/* codigos de saida da secao 11.1 da especificacao */
enum {
  SAIDA_OK = 0,
  SAIDA_USO = 1,
  SAIDA_LEXICO = 2,
  SAIDA_SINTATICO = 3,
  SAIDA_SEMANTICO = 4
};

#define USO "uso: minic [--check | --ir] [-O0 | -O1] arquivo.c\n"

static char *ler_arquivo(const char *caminho) {
  FILE *f = fopen(caminho, "rb");
  if (!f)
    return NULL;

  fseek(f, 0, SEEK_END);
  long tamanho = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (tamanho < 0) {
    fclose(f);
    return NULL;
  }

  char *buf = (char *)malloc((size_t)tamanho + 1);
  if (!buf) {
    fclose(f);
    return NULL;
  }

  size_t lidos = fread(buf, 1, (size_t)tamanho, f);
  buf[lidos] = '\0';
  fclose(f);
  return buf;
}

static void erro_lexico_stderr(const char *msg, int line, int col) {
  fprintf(stderr, "Erro lexico na linha %d, coluna %d:\n%s.\n", line, col, msg);
}

static int erros_lexicos(const char *fonte) {
  Lexer lx;
  lexer_init(&lx, fonte);
  lexer_set_error_sink(&lx, erro_lexico_stderr);
  for (;;) {
    Token t = lexer_next_token(&lx);
    int fim = t.type == TOK_EOF;
    token_free(&t);
    if (fim)
      break;
  }
  return lx.error_count;
}

int main(int argc, char **argv) {
#ifdef _WIN32
  /* senao o Windows troca \n por \r\n */
  _setmode(_fileno(stdout), _O_BINARY);
#endif

  int modo_ir = 0, otimizacao = 0;
  const char *caminho = NULL;
  int narquivos = 0;
  for (int i = 1; i < argc; i++) {
    const char *arg = argv[i];
    if (strcmp(arg, "--ir") == 0)
      modo_ir = 1;
    else if (strcmp(arg, "--check") == 0)
      modo_ir = 0;
    else if (strcmp(arg, "-O0") == 0)
      otimizacao = 0;
    else if (strcmp(arg, "-O1") == 0)
      otimizacao = 1;
    else if (arg[0] == '-') {
      fprintf(stderr, "opcao desconhecida: %s\n" USO, arg);
      return SAIDA_USO;
    } else {
      caminho = arg;
      narquivos++;
    }
  }
  if (narquivos != 1) {
    fputs(USO, stderr);
    return SAIDA_USO;
  }

  char *fonte = ler_arquivo(caminho);
  if (!fonte) {
    fprintf(stderr, "erro: nao foi possivel abrir o arquivo \"%s\"\n", caminho);
    return SAIDA_USO;
  }

  if (erros_lexicos(fonte) > 0) {
    free(fonte);
    return SAIDA_LEXICO;
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
    return SAIDA_SINTATICO;
  }

  char *saida = NULL;
  int erros = semantic_analisar(arvore, &saida);
  if (erros > 0 || !modo_ir) {
    fwrite(saida, 1, strlen(saida), stdout);
    fflush(stdout);
    free(saida);
    ast_free(arvore);
    return erros > 0 ? SAIDA_SEMANTICO : SAIDA_OK;
  }
  free(saida);

  ProgramaIR *ir = ir_gerar(arvore);
  if (otimizacao)
    opt_otimizar(ir);
  char *texto = ir_texto(ir);
  fwrite(texto, 1, strlen(texto), stdout);
  fflush(stdout);

  free(texto);
  ir_liberar(ir);
  ast_free(arvore);
  return SAIDA_OK;
}
