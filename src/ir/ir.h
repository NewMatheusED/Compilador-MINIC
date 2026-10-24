#ifndef MINIC_IR_H
#define MINIC_IR_H

#include <stddef.h>

#include "../parser/ast.h"

/* Mesma IR do lado Python (src/ir/minic_ir.py). Todas as strings vem de
 * ir_str() e ficam internadas: textos iguais tem o mesmo ponteiro, entao
 * da pra comparar com ==. */

typedef enum {
    I_COPY,    /* dst = a              */
    I_BIN,     /* dst = a opr b        */
    I_UN,      /* dst = opr a          */
    I_LOAD,    /* dst = load a[b]      */
    I_STORE,   /* store a[b], c        */
    I_LABEL,   /* opr:                 */
    I_GOTO,    /* goto opr             */
    I_IFFALSE, /* ifFalse a goto opr   */
    I_IFTRUE,  /* ifTrue a goto opr    */
    I_PARAM,   /* param a              */
    I_CALL,    /* [dst =] call opr, n  */
    I_RETURN,  /* return [a]           */
    I_PRINT,   /* print a              */
    I_READ,    /* read dst             */
    I_ARRAY,   /* array dst[a]         */
    I_GLOBAL   /* global dst[[a]]      */
} IrOp;

typedef struct {
    IrOp op;
    const char *dst, *a, *b, *c, *opr;
    int n;
} Instr;

typedef struct {
    const char *nome;
    const char **params;
    size_t nparams;
    Instr *ins;
    size_t n, cap;
} Unidade;

typedef struct {
    const char **globais;
    size_t nglobais;
    Unidade init;
    Unidade *funcoes;
    size_t nfuncoes;
} ProgramaIR;

const char *ir_str(const char *s);
void ir_add(Unidade *u, Instr i);

int ir_eh_temp(const char *s);
int ir_eh_int(const char *s);

ProgramaIR *ir_gerar(const AstNode *programa);
char *ir_texto(const ProgramaIR *p);
void ir_liberar(ProgramaIR *p);

#endif
