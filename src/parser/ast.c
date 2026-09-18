#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"

static char *dup_str(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *copia = (char *) malloc(n + 1);
    if (!copia) {
        fprintf(stderr, "erro: memoria insuficiente\n");
        exit(1);
    }
    memcpy(copia, s, n + 1);
    return copia;
}

AstNode *ast_new(NodeKind kind, const char *a, const char *b) {
    AstNode *no = (AstNode *) calloc(1, sizeof(AstNode));
    if (!no) {
        fprintf(stderr, "erro: memoria insuficiente\n");
        exit(1);
    }
    no->kind = kind;
    no->a = dup_str(a);
    no->b = dup_str(b);
    return no;
}

void ast_add(AstNode *pai, AstNode *filho) {
    if (pai->nkids == pai->cap) {
        size_t nova = pai->cap ? pai->cap * 2 : 4;
        AstNode **buf = (AstNode **) realloc(pai->kids, nova * sizeof(AstNode *));
        if (!buf) {
            fprintf(stderr, "erro: memoria insuficiente\n");
            exit(1);
        }
        pai->kids = buf;
        pai->cap = nova;
    }
    pai->kids[pai->nkids++] = filho;
}

void ast_free(AstNode *no) {
    if (!no) return;
    for (size_t i = 0; i < no->nkids; i++) ast_free(no->kids[i]);
    free(no->kids);
    free(no->a);
    free(no->b);
    free(no);
}

/* Filhos de Program e Block sao listas de comandos: separador ", ".
 * Nos demais nos o separador e "," sem espaco. */
static void print_kids(const AstNode *no, size_t inicio, size_t fim,
                       const char *sep) {
    for (size_t i = inicio; i < fim; i++) {
        if (i > inicio) fputs(sep, stdout);
        ast_print(no->kids[i]);
    }
}

void ast_print(const AstNode *no) {
    if (!no) {
        fputs("NULL", stdout);
        return;
    }

    switch (no->kind) {
        case N_PROGRAM:
            fputs("Program(", stdout);
            print_kids(no, 0, no->nkids, ", ");
            fputc(')', stdout);
            break;

        case N_BLOCK:
            fputs("Block(", stdout);
            print_kids(no, 0, no->nkids, ", ");
            fputc(')', stdout);
            break;

        case N_FUNCTION:
            printf("Function(%s %s(", no->a, no->b);
            print_kids(no, 0, no->nparams, ",");
            fputs(") ", stdout);
            ast_print(no->kids[no->nkids - 1]);
            fputc(')', stdout);
            break;

        case N_PARAM:
            printf("%s %s", no->a, no->b);
            break;

        case N_VARDECL:
            printf("VarDecl(%s %s", no->a, no->b);
            if (no->kids[0]) {
                fputs(" size=", stdout);
                ast_print(no->kids[0]);
            }
            if (no->kids[1]) {
                fputc('=', stdout);
                ast_print(no->kids[1]);
            }
            fputc(')', stdout);
            break;

        case N_IF:
            fputs("If(", stdout);
            ast_print(no->kids[0]);
            fputc(',', stdout);
            ast_print(no->kids[1]);
            fputc(',', stdout);
            ast_print(no->kids[2]);
            fputc(')', stdout);
            break;

        case N_WHILE:
            fputs("While(", stdout);
            ast_print(no->kids[0]);
            fputc(',', stdout);
            ast_print(no->kids[1]);
            fputc(')', stdout);
            break;

        case N_RETURN:
            fputs("Return(", stdout);
            ast_print(no->kids[0]);
            fputc(')', stdout);
            break;

        case N_EXPRSTMT:
            fputs("ExprStmt(", stdout);
            ast_print(no->kids[0]);
            fputc(')', stdout);
            break;

        case N_ASSIGN:
            fputs("Assign(", stdout);
            ast_print(no->kids[0]);
            fputc(',', stdout);
            ast_print(no->kids[1]);
            fputc(')', stdout);
            break;

        case N_BINARY:
            printf("Binary(%s,", no->a);
            ast_print(no->kids[0]);
            fputc(',', stdout);
            ast_print(no->kids[1]);
            fputc(')', stdout);
            break;

        case N_UNARY:
            printf("Unary(%s,", no->a);
            ast_print(no->kids[0]);
            fputc(')', stdout);
            break;

        case N_CALL:
            fputs("Call(", stdout);
            print_kids(no, 0, no->nkids, ",");
            fputc(')', stdout);
            break;

        case N_INDEX:
            fputs("Index(", stdout);
            ast_print(no->kids[0]);
            fputc(',', stdout);
            ast_print(no->kids[1]);
            fputc(')', stdout);
            break;

        case N_ID:
            printf("Id(%s)", no->a);
            break;

        case N_LIT:
            printf("Lit(%s,%s)", no->a, no->b);
            break;
    }
}
