#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ir.h"

/* O minic.c junta todos os .c numa unidade so, entao tudo aqui que e
 * static usa prefixo ir_/ger_ pra nao bater com o semantico e o parser. */

static void *ir_xalloc(void *p, size_t n) {
    p = realloc(p, n ? n : 1);
    if (!p) {
        fprintf(stderr, "erro: memoria insuficiente\n");
        exit(1);
    }
    return p;
}

#define CRESCER(vet, n, cap)                                           \
    do {                                                               \
        if ((n) == (cap)) {                                            \
            (cap) = (cap) ? (cap) * 2 : 8;                             \
            (vet) = ir_xalloc((vet), (cap) * sizeof(*(vet)));          \
        }                                                              \
    } while (0)

/* ---- strings internadas ------------------------------------------- */

static char **ir_tabela = NULL;
static size_t ir_ntabela = 0, ir_captabela = 0;

static unsigned long ir_hash(const char *s) {
    unsigned long h = 2166136261UL;
    for (; *s; s++) h = (h ^ (unsigned char) *s) * 16777619UL;
    return h;
}

static void ir_inserir_tabela(char *s) {
    size_t i = ir_hash(s) & (ir_captabela - 1);
    while (ir_tabela[i]) i = (i + 1) & (ir_captabela - 1);
    ir_tabela[i] = s;
}

const char *ir_str(const char *s) {
    if (!s) return NULL;
    if (ir_captabela == 0 || (ir_ntabela + 1) * 2 > ir_captabela) {
        char **antiga = ir_tabela;
        size_t capantiga = ir_captabela;
        ir_captabela = ir_captabela ? ir_captabela * 2 : 256;
        ir_tabela = calloc(ir_captabela, sizeof(char *));
        if (!ir_tabela) {
            fprintf(stderr, "erro: memoria insuficiente\n");
            exit(1);
        }
        for (size_t i = 0; i < capantiga; i++) {
            if (antiga[i]) ir_inserir_tabela(antiga[i]);
        }
        free(antiga);
    }
    size_t i = ir_hash(s) & (ir_captabela - 1);
    while (ir_tabela[i]) {
        if (strcmp(ir_tabela[i], s) == 0) return ir_tabela[i];
        i = (i + 1) & (ir_captabela - 1);
    }
    size_t n = strlen(s) + 1;
    char *copia = ir_xalloc(NULL, n);
    memcpy(copia, s, n);
    ir_tabela[i] = copia;
    ir_ntabela++;
    return copia;
}

static const char *ir_fmt(const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    return ir_str(buf);
}

int ir_eh_temp(const char *s) {
    if (!s || s[0] != 't' || !s[1]) return 0;
    for (s++; *s; s++) {
        if (*s < '0' || *s > '9') return 0;
    }
    return 1;
}

int ir_eh_int(const char *s) {
    if (!s) return 0;
    if (*s == '-') s++;
    if (!*s) return 0;
    for (; *s; s++) {
        if (*s < '0' || *s > '9') return 0;
    }
    return 1;
}

void ir_add(Unidade *u, Instr i) {
    CRESCER(u->ins, u->n, u->cap);
    u->ins[u->n++] = i;
}

/* ---- texto --------------------------------------------------------- */

typedef struct {
    char *s;
    size_t len, cap;
} IrSb;

static void ir_sb_printf(IrSb *sb, const char *fmt, ...) {
    va_list ap, copia;
    va_start(ap, fmt);
    va_copy(copia, ap);
    int n = vsnprintf(NULL, 0, fmt, copia);
    va_end(copia);
    if (sb->len + (size_t) n + 1 > sb->cap) {
        while (sb->len + (size_t) n + 1 > sb->cap) {
            sb->cap = sb->cap ? sb->cap * 2 : 256;
        }
        sb->s = ir_xalloc(sb->s, sb->cap);
    }
    vsnprintf(sb->s + sb->len, (size_t) n + 1, fmt, ap);
    sb->len += (size_t) n;
    va_end(ap);
}

static void ir_instr_texto(IrSb *sb, const Instr *i) {
    if (i->op != I_LABEL) ir_sb_printf(sb, "  ");
    switch (i->op) {
        case I_COPY:    ir_sb_printf(sb, "%s = %s", i->dst, i->a); break;
        case I_BIN:     ir_sb_printf(sb, "%s = %s %s %s", i->dst, i->a, i->opr, i->b); break;
        case I_UN:      ir_sb_printf(sb, "%s = %s %s", i->dst, i->opr, i->a); break;
        case I_LOAD:    ir_sb_printf(sb, "%s = load %s[%s]", i->dst, i->a, i->b); break;
        case I_STORE:   ir_sb_printf(sb, "store %s[%s], %s", i->a, i->b, i->c); break;
        case I_LABEL:   ir_sb_printf(sb, "%s:", i->opr); break;
        case I_GOTO:    ir_sb_printf(sb, "goto %s", i->opr); break;
        case I_IFFALSE: ir_sb_printf(sb, "ifFalse %s goto %s", i->a, i->opr); break;
        case I_IFTRUE:  ir_sb_printf(sb, "ifTrue %s goto %s", i->a, i->opr); break;
        case I_PARAM:   ir_sb_printf(sb, "param %s", i->a); break;
        case I_CALL:
            if (i->dst) ir_sb_printf(sb, "%s = ", i->dst);
            ir_sb_printf(sb, "call %s, %d", i->opr, i->n);
            break;
        case I_RETURN:
            ir_sb_printf(sb, i->a ? "return %s" : "return", i->a);
            break;
        case I_PRINT:   ir_sb_printf(sb, "print %s", i->a); break;
        case I_READ:    ir_sb_printf(sb, "read %s", i->dst); break;
        case I_ARRAY:   ir_sb_printf(sb, "array %s[%s]", i->dst, i->a); break;
        case I_GLOBAL:
            if (i->a) ir_sb_printf(sb, "global %s[%s]", i->dst, i->a);
            else ir_sb_printf(sb, "global %s", i->dst);
            break;
    }
    ir_sb_printf(sb, "\n");
}

char *ir_texto(const ProgramaIR *p) {
    IrSb sb = {0};
    ir_sb_printf(&sb, "%s", "");
    if (p->init.n) {
        ir_sb_printf(&sb, "init\n");
        for (size_t k = 0; k < p->init.n; k++) ir_instr_texto(&sb, &p->init.ins[k]);
        ir_sb_printf(&sb, "endinit\n");
    }
    for (size_t f = 0; f < p->nfuncoes; f++) {
        const Unidade *u = &p->funcoes[f];
        ir_sb_printf(&sb, "func %s", u->nome);
        if (u->nparams) {
            ir_sb_printf(&sb, "(");
            for (size_t k = 0; k < u->nparams; k++) {
                ir_sb_printf(&sb, k ? ", %s" : "%s", u->params[k]);
            }
            ir_sb_printf(&sb, ")");
        }
        ir_sb_printf(&sb, "\n");
        for (size_t k = 0; k < u->n; k++) ir_instr_texto(&sb, &u->ins[k]);
        ir_sb_printf(&sb, "endfunc\n");
    }
    return sb.s;
}

void ir_liberar(ProgramaIR *p) {
    free(p->globais);
    free(p->init.ins);
    for (size_t f = 0; f < p->nfuncoes; f++) {
        free(p->funcoes[f].ins);
        free(p->funcoes[f].params);
    }
    free(p->funcoes);
    free(p);
    for (size_t i = 0; i < ir_captabela; i++) free(ir_tabela[i]);
    free(ir_tabela);
    ir_tabela = NULL;
    ir_ntabela = ir_captabela = 0;
}

/* ---- gerador ------------------------------------------------------- */

typedef struct {
    const char *nome;      /* nome no fonte */
    const char *nome_ir;
    const char *tipo;
    int vetor;
} GerVar;

typedef struct {
    GerVar *vars;
    size_t n, cap;
} GerEscopo;

typedef struct {
    const char *nome;
    const char *tipo;
    const AstNode *def;
} GerFuncao;

typedef struct {
    const char *base;
    int n;
} GerRotulo;

typedef struct {
    const char *fim;
    const char *cont;
} GerLaco;

typedef struct {
    ProgramaIR *prog;
    Unidade *u;

    GerFuncao *funcoes;
    size_t nfuncoes, capfuncoes;

    GerEscopo *escopos;   /* escopos[0] = globais */
    size_t nescopos, capescopos;

    const char **usados;
    size_t nusados, capusados;
    size_t capglobais;

    int ntemp;
    GerRotulo *rotulos;
    size_t nrotulos, caprotulos;

    GerLaco *lacos;
    size_t nlacos, caplacos;

    const char *tipo_ret;
} Gerador;

static const char *ger_expr(Gerador *g, const AstNode *no);
static void ger_comando(Gerador *g, const AstNode *cmd, int global);

static int ger_usado(Gerador *g, const char *s) {
    for (size_t i = 0; i < g->nusados; i++) {
        if (g->usados[i] == s) return 1;
    }
    return 0;
}

static const char *ger_novo_nome(Gerador *g, const char *base) {
    base = ir_str(base);
    const char *nome = base;
    if (ir_eh_temp(base) || ger_usado(g, base)) {
        int k = ir_eh_temp(base) ? 1 : 2;
        while (ger_usado(g, ir_fmt("%s#%d", base, k))) k++;
        nome = ir_fmt("%s#%d", base, k);
    }
    CRESCER(g->usados, g->nusados, g->capusados);
    g->usados[g->nusados++] = nome;
    return nome;
}

static const char *ger_temp(Gerador *g) {
    return ir_fmt("t%d", ++g->ntemp);
}

static const char *ger_rotulo(Gerador *g, const char *base) {
    for (size_t i = 0; i < g->nrotulos; i++) {
        if (strcmp(g->rotulos[i].base, base) == 0) {
            return ir_fmt("%s%d", base, ++g->rotulos[i].n);
        }
    }
    CRESCER(g->rotulos, g->nrotulos, g->caprotulos);
    g->rotulos[g->nrotulos].base = base;
    g->rotulos[g->nrotulos].n = 1;
    g->nrotulos++;
    return ir_str(base);
}

static GerVar *ger_buscar(Gerador *g, const char *nome) {
    for (size_t e = g->nescopos; e > 0; e--) {
        GerEscopo *esc = &g->escopos[e - 1];
        for (size_t i = esc->n; i > 0; i--) {
            if (strcmp(esc->vars[i - 1].nome, nome) == 0) return &esc->vars[i - 1];
        }
    }
    fprintf(stderr, "erro interno: \"%s\" sem declaracao na IR\n", nome);
    exit(5);
}

static void ger_abrir(Gerador *g) {
    CRESCER(g->escopos, g->nescopos, g->capescopos);
    memset(&g->escopos[g->nescopos++], 0, sizeof(GerEscopo));
}

static void ger_fechar(Gerador *g) {
    free(g->escopos[--g->nescopos].vars);
}

static void ger_declarar(Gerador *g, const char *nome, const char *nome_ir,
                         const char *tipo, int vetor) {
    GerEscopo *esc = &g->escopos[g->nescopos - 1];
    for (size_t i = 0; i < esc->n; i++) {
        if (strcmp(esc->vars[i].nome, nome) == 0) {
            esc->vars[i].nome_ir = nome_ir;
            esc->vars[i].tipo = tipo;
            esc->vars[i].vetor = vetor;
            return;
        }
    }
    CRESCER(esc->vars, esc->n, esc->cap);
    GerVar v = {nome, nome_ir, tipo, vetor};
    esc->vars[esc->n++] = v;
}

static GerFuncao *ger_funcao(Gerador *g, const char *nome) {
    for (size_t i = 0; i < g->nfuncoes; i++) {
        if (strcmp(g->funcoes[i].nome, nome) == 0) return &g->funcoes[i];
    }
    return NULL;
}

static Instr ger_novo(IrOp op) {
    Instr i;
    memset(&i, 0, sizeof(i));
    i.op = op;
    return i;
}

static void ger_emitir(Gerador *g, Instr i) { ir_add(g->u, i); }

static void ger_emitir_bin(Gerador *g, const char *dst, const char *a,
                           const char *opr, const char *b) {
    Instr i = ger_novo(I_BIN);
    i.dst = dst; i.a = a; i.opr = ir_str(opr); i.b = b;
    ger_emitir(g, i);
}

static void ger_emitir_un(Gerador *g, const char *dst, const char *opr,
                          const char *a) {
    Instr i = ger_novo(I_UN);
    i.dst = dst; i.opr = ir_str(opr); i.a = a;
    ger_emitir(g, i);
}

static void ger_emitir_copy(Gerador *g, const char *dst, const char *a) {
    Instr i = ger_novo(I_COPY);
    i.dst = dst; i.a = a;
    ger_emitir(g, i);
}

static void ger_emitir_salto(Gerador *g, IrOp op, const char *a,
                             const char *rotulo) {
    Instr i = ger_novo(op);
    i.a = a; i.opr = rotulo;
    ger_emitir(g, i);
}

static void ger_emitir_simples(Gerador *g, IrOp op, const char *a) {
    Instr i = ger_novo(op);
    if (op == I_READ) i.dst = a;
    else i.a = a;
    ger_emitir(g, i);
}

/* ---- tipos --------------------------------------------------------- */

static const char *ger_tipo(Gerador *g, const AstNode *no) {
    switch (no->kind) {
        case N_LIT:
            return strcmp(no->a, "real") == 0 ? "float" : no->a;
        case N_ID:
            return ger_buscar(g, no->a)->tipo;
        case N_INDEX:
            return ger_tipo(g, no->kids[0]);
        case N_ASSIGN:
            return ger_tipo(g, no->kids[0]);
        case N_CALL:
            return ger_funcao(g, no->kids[0]->a)->tipo;
        case N_UNARY:
            if (strcmp(no->a, "!") == 0) return "bool";
            return strcmp(ger_tipo(g, no->kids[0]), "float") == 0 ? "float" : "int";
        case N_BINARY: {
            const char *op = no->a;
            if (strcmp(op, "+") == 0 || strcmp(op, "-") == 0
                || strcmp(op, "*") == 0 || strcmp(op, "/") == 0) {
                int f = strcmp(ger_tipo(g, no->kids[0]), "float") == 0
                        || strcmp(ger_tipo(g, no->kids[1]), "float") == 0;
                return f ? "float" : "int";
            }
            if (strcmp(op, "%") == 0) return "int";
            return "bool";
        }
        default:
            return "int";
    }
}

static const char *ger_converter(Gerador *g, const char *valor,
                                 const char *de, const char *para) {
    if (strcmp(para, "float") != 0
        || (strcmp(de, "int") != 0 && strcmp(de, "char") != 0)) {
        return valor;
    }
    if (ir_eh_int(valor)) return ir_fmt("%s.0", valor);
    const char *t = ger_temp(g);
    ger_emitir_un(g, t, "itof", valor);
    return t;
}

/* ---- expressoes ---------------------------------------------------- */

static int ger_simples(const AstNode *no) {
    switch (no->kind) {
        case N_LIT:
        case N_ID:
            return 1;
        case N_UNARY:
            return ger_simples(no->kids[0]);
        case N_BINARY:
            return strcmp(no->a, "/") != 0 && strcmp(no->a, "%") != 0
                   && ger_simples(no->kids[0]) && ger_simples(no->kids[1]);
        default:
            return 0;
    }
}

static const char *ger_deslocamento(Gerador *g, const AstNode *indice) {
    const char *i = ger_expr(g, indice);
    const char *t = ger_temp(g);
    ger_emitir_bin(g, t, i, "*", ir_str("1"));
    return t;
}

static const char *ger_atribuicao(Gerador *g, const AstNode *no) {
    const AstNode *destino = no->kids[0];
    const char *tipo_dest = ger_tipo(g, destino);
    if (destino->kind == N_ID) {
        const char *nome = ger_buscar(g, destino->a)->nome_ir;
        const char *v = ger_expr(g, no->kids[1]);
        v = ger_converter(g, v, ger_tipo(g, no->kids[1]), tipo_dest);
        ger_emitir_copy(g, nome, v);
        return nome;
    }
    const char *vetor = ger_buscar(g, destino->kids[0]->a)->nome_ir;
    const char *desl = ger_deslocamento(g, destino->kids[1]);
    const char *v = ger_expr(g, no->kids[1]);
    v = ger_converter(g, v, ger_tipo(g, no->kids[1]), tipo_dest);
    Instr i = ger_novo(I_STORE);
    i.a = vetor; i.b = desl; i.c = v;
    ger_emitir(g, i);
    return v;
}

static const char *ger_curto_circuito(Gerador *g, const AstNode *no) {
    int e = strcmp(no->a, "&&") == 0;
    const char *a = ger_expr(g, no->kids[0]);
    const char *t = ger_temp(g);
    ger_emitir_copy(g, t, a);
    const char *fim = ger_rotulo(g, "L_sc");
    ger_emitir_salto(g, e ? I_IFFALSE : I_IFTRUE, t, fim);
    const char *b = ger_expr(g, no->kids[1]);
    ger_emitir_copy(g, t, b);
    ger_emitir_salto(g, I_LABEL, NULL, fim);
    return t;
}

static const char *ger_binaria(Gerador *g, const AstNode *no) {
    const char *op = no->a;
    int logico = strcmp(op, "&&") == 0 || strcmp(op, "||") == 0;
    if (logico && !ger_simples(no->kids[1])) return ger_curto_circuito(g, no);
    const char *a = ger_expr(g, no->kids[0]);
    const char *b = ger_expr(g, no->kids[1]);
    const char *te = ger_tipo(g, no->kids[0]);
    const char *td = ger_tipo(g, no->kids[1]);
    if (!logico && (strcmp(te, "float") == 0 || strcmp(td, "float") == 0)) {
        a = ger_converter(g, a, te, "float");
        b = ger_converter(g, b, td, "float");
    }
    if (strcmp(op, "&&") == 0) op = "and";
    else if (strcmp(op, "||") == 0) op = "or";
    const char *t = ger_temp(g);
    ger_emitir_bin(g, t, a, op, b);
    return t;
}

static const char *ger_chamada(Gerador *g, const AstNode *no) {
    const char *nome = ir_str(no->kids[0]->a);
    GerFuncao *f = ger_funcao(g, nome);
    const AstNode *def = f->def;
    size_t nargs = no->nkids - 1;
    const char **args = ir_xalloc(NULL, nargs * sizeof(char *));
    for (size_t k = 0; k < nargs; k++) {
        const AstNode *arg = no->kids[k + 1];
        const AstNode *p = def->kids[k];
        const char *v = ger_expr(g, arg);
        if (!p->vetor) v = ger_converter(g, v, ger_tipo(g, arg), p->a);
        args[k] = v;
    }
    for (size_t k = 0; k < nargs; k++) ger_emitir_simples(g, I_PARAM, args[k]);
    free(args);
    Instr i = ger_novo(I_CALL);
    i.opr = nome;
    i.n = (int) nargs;
    if (strcmp(f->tipo, "void") == 0) {
        ger_emitir(g, i);
        return NULL;
    }
    i.dst = ger_temp(g);
    ger_emitir(g, i);
    return i.dst;
}

static const char *ger_expr(Gerador *g, const AstNode *no) {
    switch (no->kind) {
        case N_LIT:
            return ir_str(no->b);
        case N_ID:
            return ger_buscar(g, no->a)->nome_ir;
        case N_ASSIGN:
            return ger_atribuicao(g, no);
        case N_BINARY:
            return ger_binaria(g, no);
        case N_UNARY: {
            const char *a = ger_expr(g, no->kids[0]);
            if (strcmp(no->a, "+") == 0) return a;
            const char *t = ger_temp(g);
            ger_emitir_un(g, t, strcmp(no->a, "-") == 0 ? "neg" : "not", a);
            return t;
        }
        case N_CALL:
            return ger_chamada(g, no);
        case N_INDEX: {
            const char *vetor = ger_buscar(g, no->kids[0]->a)->nome_ir;
            const char *desl = ger_deslocamento(g, no->kids[1]);
            Instr i = ger_novo(I_LOAD);
            i.dst = ger_temp(g);
            i.a = vetor;
            i.b = desl;
            ger_emitir(g, i);
            return i.dst;
        }
        default:
            fprintf(stderr, "erro interno: expressao sem IR\n");
            exit(5);
    }
}

/* ---- comandos ------------------------------------------------------ */

static void ger_decl_var(Gerador *g, const AstNode *d, int global) {
    const char *tamanho = d->kids[0] ? ger_expr(g, d->kids[0]) : NULL;
    const char *valor = d->kids[1] ? ger_expr(g, d->kids[1]) : NULL;
    const char *nome = ger_novo_nome(g, d->b);
    ger_declarar(g, d->b, nome, d->a, tamanho != NULL);
    if (global) {
        CRESCER(g->prog->globais, g->prog->nglobais, g->capglobais);
        g->prog->globais[g->prog->nglobais++] = nome;
        Instr i = ger_novo(I_GLOBAL);
        i.dst = nome;
        i.a = tamanho;
        ger_emitir(g, i);
    } else if (tamanho) {
        Instr i = ger_novo(I_ARRAY);
        i.dst = nome;
        i.a = tamanho;
        ger_emitir(g, i);
    }
    if (valor) {
        valor = ger_converter(g, valor, ger_tipo(g, d->kids[1]), d->a);
        ger_emitir_copy(g, nome, valor);
    }
}

static void ger_if(Gerador *g, const AstNode *cmd) {
    const char *cond = ger_expr(g, cmd->kids[0]);
    if (!cmd->kids[2]) {
        const char *fim = ger_rotulo(g, "L_end_if");
        ger_emitir_salto(g, I_IFFALSE, cond, fim);
        ger_comando(g, cmd->kids[1], 0);
        ger_emitir_salto(g, I_LABEL, NULL, fim);
        return;
    }
    const char *senao = ger_rotulo(g, "L_else");
    const char *fim = ger_rotulo(g, "L_end");
    ger_emitir_salto(g, I_IFFALSE, cond, senao);
    ger_comando(g, cmd->kids[1], 0);
    ger_emitir_salto(g, I_GOTO, NULL, fim);
    ger_emitir_salto(g, I_LABEL, NULL, senao);
    ger_comando(g, cmd->kids[2], 0);
    ger_emitir_salto(g, I_LABEL, NULL, fim);
}

static void ger_laco(Gerador *g, const AstNode *cond, const AstNode *corpo,
                     const AstNode *passo) {
    const char *teste = ger_rotulo(g, "L_test");
    const char *fim = ger_rotulo(g, "L_end");
    ger_emitir_salto(g, I_LABEL, NULL, teste);
    if (cond) ger_emitir_salto(g, I_IFFALSE, ger_expr(g, cond), fim);

    CRESCER(g->lacos, g->nlacos, g->caplacos);
    size_t meu = g->nlacos++;
    g->lacos[meu].fim = fim;
    g->lacos[meu].cont = passo ? NULL : teste;
    ger_comando(g, corpo, 0);
    const char *cont = g->lacos[meu].cont;
    g->nlacos--;

    if (passo) {
        if (cont) ger_emitir_salto(g, I_LABEL, NULL, cont);
        ger_expr(g, passo);
    }
    ger_emitir_salto(g, I_GOTO, NULL, teste);
    ger_emitir_salto(g, I_LABEL, NULL, fim);
}

static void ger_read(Gerador *g, const AstNode *destino) {
    if (destino->kind == N_ID) {
        ger_emitir_simples(g, I_READ, ger_buscar(g, destino->a)->nome_ir);
        return;
    }
    const char *vetor = ger_buscar(g, destino->kids[0]->a)->nome_ir;
    const char *desl = ger_deslocamento(g, destino->kids[1]);
    const char *t = ger_temp(g);
    ger_emitir_simples(g, I_READ, t);
    Instr i = ger_novo(I_STORE);
    i.a = vetor; i.b = desl; i.c = t;
    ger_emitir(g, i);
}

static void ger_comando(Gerador *g, const AstNode *cmd, int global) {
    switch (cmd->kind) {
        case N_VARDECL:
            ger_decl_var(g, cmd, global);
            break;
        case N_BLOCK:
            ger_abrir(g);
            for (size_t k = 0; k < cmd->nkids; k++) ger_comando(g, cmd->kids[k], 0);
            ger_fechar(g);
            break;
        case N_IF:
            ger_if(g, cmd);
            break;
        case N_WHILE:
            ger_laco(g, cmd->kids[0], cmd->kids[1], NULL);
            break;
        case N_FOR:
            if (cmd->kids[0]) ger_expr(g, cmd->kids[0]);
            ger_laco(g, cmd->kids[1], cmd->kids[3], cmd->kids[2]);
            break;
        case N_BREAK:
            ger_emitir_salto(g, I_GOTO, NULL, g->lacos[g->nlacos - 1].fim);
            break;
        case N_CONTINUE: {
            GerLaco *ctx = &g->lacos[g->nlacos - 1];
            if (!ctx->cont) ctx->cont = ger_rotulo(g, "L_step");
            ger_emitir_salto(g, I_GOTO, NULL, ctx->cont);
            break;
        }
        case N_RETURN: {
            Instr i = ger_novo(I_RETURN);
            if (cmd->kids[0]) {
                const char *v = ger_expr(g, cmd->kids[0]);
                i.a = ger_converter(g, v, ger_tipo(g, cmd->kids[0]), g->tipo_ret);
            }
            ger_emitir(g, i);
            break;
        }
        case N_PRINT:
            ger_emitir_simples(g, I_PRINT, ger_expr(g, cmd->kids[0]));
            break;
        case N_READ:
            ger_read(g, cmd->kids[0]);
            break;
        case N_EXPRSTMT:
            ger_expr(g, cmd->kids[0]);
            break;
        default:
            break;
    }
}

static void ger_funcao_def(Gerador *g, const AstNode *f) {
    ProgramaIR *p = g->prog;
    p->funcoes = ir_xalloc(p->funcoes, (p->nfuncoes + 1) * sizeof(Unidade));
    Unidade *u = &p->funcoes[p->nfuncoes++];
    memset(u, 0, sizeof(*u));
    u->nome = ir_str(f->b);
    g->u = u;

    g->nusados = 0;
    for (size_t k = 0; k < p->nglobais; k++) {
        CRESCER(g->usados, g->nusados, g->capusados);
        g->usados[g->nusados++] = p->globais[k];
    }
    g->ntemp = 0;
    g->nrotulos = 0;
    g->tipo_ret = f->a;

    ger_abrir(g);
    u->params = ir_xalloc(NULL, f->nparams * sizeof(char *));
    u->nparams = f->nparams;
    for (size_t k = 0; k < f->nparams; k++) {
        const AstNode *par = f->kids[k];
        const char *nome = ger_novo_nome(g, par->b);
        ger_declarar(g, par->b, nome, par->a, par->vetor);
        u->params[k] = par->vetor ? ir_fmt("%s[]", nome) : nome;
    }
    const AstNode *corpo = f->kids[f->nkids - 1];
    for (size_t k = 0; k < corpo->nkids; k++) ger_comando(g, corpo->kids[k], 0);
    ger_fechar(g);

    if (strcmp(f->a, "void") == 0 && (u->n == 0 || u->ins[u->n - 1].op != I_RETURN)) {
        ger_emitir(g, ger_novo(I_RETURN));
    }
}

ProgramaIR *ir_gerar(const AstNode *programa) {
    Gerador g;
    memset(&g, 0, sizeof(g));
    g.prog = ir_xalloc(NULL, sizeof(ProgramaIR));
    memset(g.prog, 0, sizeof(ProgramaIR));
    g.prog->init.nome = ir_str("init");
    g.u = &g.prog->init;
    g.tipo_ret = "void";

    for (size_t k = 0; k < programa->nkids; k++) {
        const AstNode *item = programa->kids[k];
        if (item->kind == N_FUNCTION && !ger_funcao(&g, item->b)) {
            CRESCER(g.funcoes, g.nfuncoes, g.capfuncoes);
            GerFuncao f = {item->b, item->a, item};
            g.funcoes[g.nfuncoes++] = f;
        }
    }

    ger_abrir(&g);
    for (size_t k = 0; k < programa->nkids; k++) {
        const AstNode *item = programa->kids[k];
        if (item->kind != N_FUNCTION) ger_comando(&g, item, 1);
    }
    for (size_t k = 0; k < programa->nkids; k++) {
        const AstNode *item = programa->kids[k];
        if (item->kind == N_FUNCTION) ger_funcao_def(&g, item);
    }
    ger_fechar(&g);

    free(g.funcoes);
    free(g.escopos);
    free(g.usados);
    free(g.rotulos);
    free(g.lacos);
    return g.prog;
}
