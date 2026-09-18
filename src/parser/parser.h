#ifndef MINIC_PARSER_H
#define MINIC_PARSER_H

#include "ast.h"

/* Analisador sintatico descendente recursivo do MINIC.
 *
 * Consome tokens sob demanda do lexer da etapa 1 (src/lexer/lexer.h),
 * mantendo apenas um pequeno buffer de lookahead.
 */

typedef struct {
    char mensagem[512];
    int linha;
    int coluna;
} ErroSintatico;

/* Retorna a AST em caso de sucesso, ou NULL preenchendo *erro.
 * O chamador libera a arvore com ast_free(). */
AstNode *parse_source(const char *src, ErroSintatico *erro);

#endif
