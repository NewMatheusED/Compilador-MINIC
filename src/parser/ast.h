#ifndef MINIC_AST_H
#define MINIC_AST_H

#include <stddef.h>

/* Nos da AST do MINIC. A impressao segue a mesma notacao de S-expressao
 * usada pelo lado Python (src/parser/minic_ast.py):
 *
 *   Program, Function, Block, VarDecl, If, While, Return, ExprStmt,
 *   Assign, Binary, Unary, Call, Index, Id, Lit
 */

typedef enum {
    N_PROGRAM,
    N_FUNCTION,
    N_PARAM,
    N_BLOCK,
    N_VARDECL,
    N_IF,
    N_WHILE,
    N_RETURN,
    N_EXPRSTMT,
    N_ASSIGN,
    N_BINARY,
    N_UNARY,
    N_CALL,
    N_INDEX,
    N_ID,
    N_LIT
} NodeKind;

typedef struct AstNode {
    NodeKind kind;

    /* Campos textuais, conforme o tipo do no:
     *   N_FUNCTION / N_PARAM / N_VARDECL : a = tipo,          b = nome
     *   N_ID                             : a = identificador
     *   N_LIT                            : a = tipo (int/real/bool/...),
     *                                      b = lexema
     *   N_BINARY / N_UNARY               : a = operador
     */
    char *a;
    char *b;

    struct AstNode **kids;
    size_t nkids;
    size_t cap;

    /* N_FUNCTION: quantos filhos iniciais sao parametros (o ultimo filho
     * e sempre o corpo).  N_VARDECL usa kids[0] = tamanho e kids[1] =
     * inicializador, qualquer um podendo ser NULL. */
    size_t nparams;
} AstNode;

AstNode *ast_new(NodeKind kind, const char *a, const char *b);
void ast_add(AstNode *pai, AstNode *filho);
void ast_free(AstNode *no);

/* Imprime a AST em uma unica linha, sem quebra de linha ao final. */
void ast_print(const AstNode *no);

#endif
