#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "opt.h"

/* Mesmos passos e mesma ordem do lado Python (src/optimizer/minic_opt.py),
 * pra os dois imprimirem a mesma IR otimizada. */

static void *opt_xalloc(void *p, size_t n) {
    p = realloc(p, n ? n : 1);
    if (!p) {
        fprintf(stderr, "erro: memoria insuficiente\n");
        exit(1);
    }
    return p;
}

#define OPT_CRESCER(vet, n, cap)                                       \
    do {                                                               \
        if ((n) == (cap)) {                                            \
            (cap) = (cap) ? (cap) * 2 : 8;                             \
            (vet) = opt_xalloc((vet), (cap) * sizeof(*(vet)));         \
        }                                                              \
    } while (0)

#define INT_MIN32 (-2147483647LL - 1)

/* ---- constantes ---------------------------------------------------- */

static int opt_eh_const(const char *x) {
    if (!x) return 0;
    if ((x[0] >= '0' && x[0] <= '9') || x[0] == '-' || x[0] == '\''
        || x[0] == '"') return 1;
    return strcmp(x, "true") == 0 || strcmp(x, "false") == 0;
}

static int opt_eh_bool(const char *x) {
    return strcmp(x, "true") == 0 || strcmp(x, "false") == 0;
}

static int opt_eh_float(const char *x) {
    if (*x == '-') x++;
    if (*x < '0' || *x > '9') return 0;
    while (*x >= '0' && *x <= '9') x++;
    if (*x++ != '.') return 0;
    if (*x < '0' || *x > '9') return 0;
    while (*x >= '0' && *x <= '9') x++;
    return *x == '\0';
}

static long long opt_wrap(long long v) {
    unsigned long long u = (unsigned long long) v & 0xffffffffULL;
    return u >= 0x80000000ULL ? (long long) u - 0x100000000LL : (long long) u;
}

/* le o literal ja reduzido a 32 bits, como o wrap(int(a)) do Python */
static long long opt_int(const char *s) {
    int neg = *s == '-';
    if (neg) s++;
    unsigned long long u = 0;
    for (; *s; s++) u = (u * 10 + (unsigned) (*s - '0')) & 0xffffffffULL;
    long long v = opt_wrap((long long) u);
    return opt_wrap(neg ? -v : v);
}

static const char *opt_num(long long v) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%lld", v);
    return ir_str(buf);
}

static const char *opt_bool(int v) { return ir_str(v ? "true" : "false"); }

static int opt_eq(const char *a, const char *b) { return strcmp(a, b) == 0; }

static const char *opt_dobrar_bin(const char *op, const char *a, const char *b) {
    if (ir_eh_int(a) && ir_eh_int(b)) {
        long long x = opt_int(a), y = opt_int(b);
        if (opt_eq(op, "+")) return opt_num(opt_wrap(x + y));
        if (opt_eq(op, "-")) return opt_num(opt_wrap(x - y));
        if (opt_eq(op, "*")) return opt_num(opt_wrap(x * y));
        if (opt_eq(op, "/") || opt_eq(op, "%")) {
            if (y == 0 || (x == INT_MIN32 && y == -1)) return NULL;
            long long q = (x < 0 ? -x : x) / (y < 0 ? -y : y);
            if ((x < 0) != (y < 0)) q = -q;
            return opt_num(opt_eq(op, "/") ? q : x - y * q);
        }
        if (opt_eq(op, "<"))  return opt_bool(x < y);
        if (opt_eq(op, "<=")) return opt_bool(x <= y);
        if (opt_eq(op, ">"))  return opt_bool(x > y);
        if (opt_eq(op, ">=")) return opt_bool(x >= y);
        if (opt_eq(op, "==")) return opt_bool(x == y);
        if (opt_eq(op, "!=")) return opt_bool(x != y);
        return NULL;
    }
    if (opt_eh_bool(a) && opt_eh_bool(b)) {
        int x = opt_eq(a, "true"), y = opt_eq(b, "true");
        if (opt_eq(op, "and")) return opt_bool(x && y);
        if (opt_eq(op, "or"))  return opt_bool(x || y);
        if (opt_eq(op, "==")) return opt_bool(x == y);
        if (opt_eq(op, "!=")) return opt_bool(x != y);
    }
    return NULL;
}

static const char *opt_identidade(const char *op, const char *a, const char *b) {
    if (opt_eq(op, "+")) {
        if (opt_eq(a, "0")) return b;
        if (opt_eq(b, "0")) return a;
    } else if (opt_eq(op, "-")) {
        if (opt_eq(b, "0")) return a;
    } else if (opt_eq(op, "*")) {
        if (opt_eq(a, "1")) return b;
        if (opt_eq(b, "1")) return a;
        if (opt_eq(a, "0") || opt_eq(b, "0")) return ir_str("0");
    } else if (opt_eq(op, "/")) {
        if (opt_eq(b, "1")) return a;
    } else if (opt_eq(op, "and")) {
        if (opt_eq(a, "true")) return b;
        if (opt_eq(b, "true")) return a;
        if (opt_eq(a, "false") || opt_eq(b, "false")) return ir_str("false");
    } else if (opt_eq(op, "or")) {
        if (opt_eq(a, "false")) return b;
        if (opt_eq(b, "false")) return a;
        if (opt_eq(a, "true") || opt_eq(b, "true")) return ir_str("true");
    }
    return NULL;
}

static const char *opt_dobrar_un(const char *op, const char *a) {
    char buf[600];
    if (opt_eq(op, "neg")) {
        if (ir_eh_int(a)) return opt_num(opt_wrap(-opt_int(a)));
        if (opt_eh_float(a)) {
            if (a[0] == '-') return ir_str(a + 1);
            snprintf(buf, sizeof(buf), "-%s", a);
            return ir_str(buf);
        }
    } else if (opt_eq(op, "not") && opt_eh_bool(a)) {
        return opt_bool(!opt_eq(a, "true"));
    } else if (opt_eq(op, "itof") && ir_eh_int(a)) {
        snprintf(buf, sizeof(buf), "%lld.0", opt_int(a));
        return ir_str(buf);
    }
    return NULL;
}

/* ---- conjuntos de nomes -------------------------------------------- */

typedef struct {
    const char **v;
    size_t n, cap;
} OptConj;

static int conj_tem(const OptConj *c, const char *x) {
    for (size_t i = 0; i < c->n; i++) {
        if (c->v[i] == x) return 1;
    }
    return 0;
}

static void conj_add(OptConj *c, const char *x) {
    if (!x || conj_tem(c, x)) return;
    OPT_CRESCER(c->v, c->n, c->cap);
    c->v[c->n++] = x;
}

static void conj_del(OptConj *c, const char *x) {
    for (size_t i = 0; i < c->n; i++) {
        if (c->v[i] == x) {
            c->v[i] = c->v[--c->n];
            return;
        }
    }
}

static void conj_uniao(OptConj *c, const OptConj *o) {
    for (size_t i = 0; i < o->n; i++) conj_add(c, o->v[i]);
}

static int conj_igual(const OptConj *a, const OptConj *b) {
    if (a->n != b->n) return 0;
    for (size_t i = 0; i < a->n; i++) {
        if (!conj_tem(b, a->v[i])) return 0;
    }
    return 1;
}

static void conj_copiar(OptConj *dst, const OptConj *src) {
    dst->n = 0;
    conj_uniao(dst, src);
}

/* ---- usos e definicoes --------------------------------------------- */

static size_t opt_usos(const Instr *i, const char *out[2]) {
    const char *lista[2] = {NULL, NULL};
    switch (i->op) {
        case I_COPY: case I_UN: case I_IFFALSE: case I_IFTRUE: case I_PARAM:
        case I_PRINT: case I_RETURN: case I_ARRAY: case I_GLOBAL:
            lista[0] = i->a;
            break;
        case I_BIN:
            lista[0] = i->a;
            lista[1] = i->b;
            break;
        case I_LOAD:
            lista[0] = i->b;
            break;
        case I_STORE:
            lista[0] = i->b;
            lista[1] = i->c;
            break;
        default:
            break;
    }
    size_t n = 0;
    for (int k = 0; k < 2; k++) {
        if (lista[k] && !opt_eh_const(lista[k])) out[n++] = lista[k];
    }
    return n;
}

static const char *opt_definida(const Instr *i) {
    switch (i->op) {
        case I_COPY: case I_BIN: case I_UN: case I_LOAD: case I_READ: case I_CALL:
            return i->dst;
        default:
            return NULL;
    }
}

static int opt_salto(IrOp op) {
    return op == I_GOTO || op == I_IFFALSE || op == I_IFTRUE;
}

typedef const char *(*OptTroca)(void *ctx, const char *x);

static int opt_substituir(Instr *i, OptTroca troca, void *ctx) {
    const char **campos[2] = {NULL, NULL};
    switch (i->op) {
        case I_COPY: case I_UN: case I_IFFALSE: case I_IFTRUE: case I_PARAM:
        case I_PRINT: case I_RETURN: case I_ARRAY: case I_GLOBAL:
            campos[0] = &i->a;
            break;
        case I_BIN:
            campos[0] = &i->a;
            campos[1] = &i->b;
            break;
        case I_LOAD:
            campos[0] = &i->b;
            break;
        case I_STORE:
            campos[0] = &i->b;
            campos[1] = &i->c;
            break;
        default:
            break;
    }
    int mudou = 0;
    for (int k = 0; k < 2; k++) {
        if (!campos[k]) continue;
        const char *x = *campos[k];
        if (!x || opt_eh_const(x)) continue;
        const char *y = troca(ctx, x);
        if (y != x) {
            *campos[k] = y;
            mudou = 1;
        }
    }
    return mudou;
}

/* ---- grafo de fluxo ------------------------------------------------ */

typedef struct {
    size_t nb;
    size_t *ini, *fim;
    int (*suc)[2];
    int *nsuc;
    int **pred;
    int *npred;
    char *alc;
    size_t nalc;
} OptGrafo;

static void grafo_montar(OptGrafo *g, const Instr *ins, size_t n) {
    memset(g, 0, sizeof(*g));
    char *lider = calloc(n + 1, 1);
    if (n) lider[0] = 1;
    for (size_t k = 0; k < n; k++) {
        if (ins[k].op == I_LABEL) lider[k] = 1;
        else if ((opt_salto(ins[k].op) || ins[k].op == I_RETURN) && k + 1 < n) lider[k + 1] = 1;
    }
    for (size_t k = 0; k < n; k++) g->nb += lider[k];
    size_t nb = g->nb;
    g->ini = opt_xalloc(NULL, nb * sizeof(size_t));
    g->fim = opt_xalloc(NULL, nb * sizeof(size_t));
    g->suc = opt_xalloc(NULL, nb * sizeof(int[2]));
    g->nsuc = calloc(nb + 1, sizeof(int));
    g->pred = calloc(nb + 1, sizeof(int *));
    g->npred = calloc(nb + 1, sizeof(int));
    g->alc = calloc(nb + 1, 1);
    size_t b = 0;
    for (size_t k = 0; k < n; k++) {
        if (!lider[k]) continue;
        if (b > 0) g->fim[b - 1] = k;
        g->ini[b++] = k;
    }
    if (nb) g->fim[nb - 1] = n;
    free(lider);

    for (b = 0; b < nb; b++) {
        const Instr *ult = &ins[g->fim[b] - 1];
        if (ult->op != I_GOTO && ult->op != I_RETURN && b + 1 < nb) {
            g->suc[b][g->nsuc[b]++] = (int) b + 1;
        }
        if (opt_salto(ult->op)) {
            int alvo = -1;
            for (size_t c = 0; c < nb; c++) {
                const Instr *pri = &ins[g->ini[c]];
                if (pri->op == I_LABEL && pri->opr == ult->opr) {
                    alvo = (int) c;
                    break;
                }
            }
            if (alvo >= 0 && !(g->nsuc[b] == 1 && g->suc[b][0] == alvo)) {
                g->suc[b][g->nsuc[b]++] = alvo;
            }
        }
    }
    for (b = 0; b < nb; b++) {
        for (int s = 0; s < g->nsuc[b]; s++) {
            int x = g->suc[b][s];
            g->pred[x] = opt_xalloc(g->pred[x], (size_t) (g->npred[x] + 1) * sizeof(int));
            g->pred[x][g->npred[x]++] = (int) b;
        }
    }
    if (nb) {
        int *pilha = opt_xalloc(NULL, (2 * nb + 1) * sizeof(int));
        size_t np = 0;
        pilha[np++] = 0;
        while (np) {
            int x = pilha[--np];
            if (g->alc[x]) continue;
            g->alc[x] = 1;
            g->nalc++;
            for (int s = 0; s < g->nsuc[x]; s++) pilha[np++] = g->suc[x][s];
        }
        free(pilha);
    }
}

static void grafo_liberar(OptGrafo *g) {
    for (size_t b = 0; b < g->nb; b++) free(g->pred[b]);
    free(g->ini); free(g->fim); free(g->suc); free(g->nsuc);
    free(g->pred); free(g->npred); free(g->alc);
}

/* ---- dobra e identidades ------------------------------------------- */

static int opt_simplificar(Unidade *u, const OptConj *G) {
    (void) G;
    int mudou = 0;
    size_t m = 0;
    for (size_t k = 0; k < u->n; k++) {
        Instr i = u->ins[k];
        const char *r = NULL;
        if (i.op == I_BIN) {
            r = opt_dobrar_bin(i.opr, i.a, i.b);
            if (!r) r = opt_identidade(i.opr, i.a, i.b);
        } else if (i.op == I_UN) {
            r = opt_dobrar_un(i.opr, i.a);
        } else if ((i.op == I_IFFALSE || i.op == I_IFTRUE) && opt_eh_bool(i.a)) {
            mudou = 1;
            if (opt_eq(i.a, "true") == (i.op == I_IFTRUE)) {
                Instr j;
                memset(&j, 0, sizeof(j));
                j.op = I_GOTO;
                j.opr = i.opr;
                u->ins[m++] = j;
            }
            continue;
        }
        if (r) {
            Instr j;
            memset(&j, 0, sizeof(j));
            j.op = I_COPY;
            j.dst = i.dst;
            j.a = r;
            i = j;
            mudou = 1;
        }
        u->ins[m++] = i;
    }
    u->n = m;
    return mudou;
}

/* ---- copias disponiveis -------------------------------------------- */

typedef struct {
    const char **x, **y;
    size_t n, cap;
    int valido;
} OptFatos;

static const char *fatos_get(const OptFatos *f, const char *x) {
    for (size_t i = 0; i < f->n; i++) {
        if (f->x[i] == x) return f->y[i];
    }
    return NULL;
}

static void fatos_set(OptFatos *f, const char *x, const char *y) {
    for (size_t i = 0; i < f->n; i++) {
        if (f->x[i] == x) {
            f->y[i] = y;
            return;
        }
    }
    if (f->n == f->cap) {
        f->cap = f->cap ? f->cap * 2 : 8;
        f->x = opt_xalloc(f->x, f->cap * sizeof(char *));
        f->y = opt_xalloc(f->y, f->cap * sizeof(char *));
    }
    f->x[f->n] = x;
    f->y[f->n] = y;
    f->n++;
}

static void fatos_copiar(OptFatos *dst, const OptFatos *src) {
    dst->n = 0;
    dst->valido = src->valido;
    for (size_t i = 0; i < src->n; i++) fatos_set(dst, src->x[i], src->y[i]);
}

static int fatos_igual(const OptFatos *a, const OptFatos *b) {
    if (a->valido != b->valido || a->n != b->n) return 0;
    for (size_t i = 0; i < a->n; i++) {
        if (fatos_get(b, a->x[i]) != a->y[i]) return 0;
    }
    return 1;
}

static void fatos_filtrar_def(OptFatos *f, const char *d) {
    size_t m = 0;
    for (size_t i = 0; i < f->n; i++) {
        if (f->x[i] == d || f->y[i] == d) continue;
        f->x[m] = f->x[i];
        f->y[m] = f->y[i];
        m++;
    }
    f->n = m;
}

static void fatos_filtrar_globais(OptFatos *f, const OptConj *G) {
    size_t m = 0;
    for (size_t i = 0; i < f->n; i++) {
        if (conj_tem(G, f->x[i]) || conj_tem(G, f->y[i])) continue;
        f->x[m] = f->x[i];
        f->y[m] = f->y[i];
        m++;
    }
    f->n = m;
}

static void fatos_liberar(OptFatos *f) {
    free(f->x);
    free(f->y);
}

static void opt_transferir(OptFatos *f, const Instr *i, const OptConj *G) {
    const char *d = opt_definida(i);
    if (d) fatos_filtrar_def(f, d);
    if (i->op == I_CALL) fatos_filtrar_globais(f, G);
    if (i->op == I_COPY && i->dst != i->a) fatos_set(f, i->dst, i->a);
}

static const char *opt_troca_fatos(void *ctx, const char *x) {
    const char *y = fatos_get((const OptFatos *) ctx, x);
    return y ? y : x;
}

static int opt_propagar(Unidade *u, const OptConj *G) {
    OptGrafo g;
    grafo_montar(&g, u->ins, u->n);
    size_t nb = g.nb;
    OptFatos *ent = calloc(nb + 1, sizeof(OptFatos));
    OptFatos *sai = calloc(nb + 1, sizeof(OptFatos));
    OptFatos tmp = {0};

    int mudou = 1;
    while (mudou) {
        mudou = 0;
        for (size_t b = 0; b < nb; b++) {
            OptFatos *e = &ent[b];
            e->n = 0;
            e->valido = 0;
            if (b == 0 || g.npred[b] == 0) {
                e->valido = 1;
            } else {
                for (int k = 0; k < g.npred[b]; k++) {
                    OptFatos *s = &sai[g.pred[b][k]];
                    if (!s->valido) continue;
                    if (!e->valido) {
                        fatos_copiar(e, s);
                        continue;
                    }
                    size_t m = 0;
                    for (size_t i = 0; i < e->n; i++) {
                        if (fatos_get(s, e->x[i]) != e->y[i]) continue;
                        e->x[m] = e->x[i];
                        e->y[m] = e->y[i];
                        m++;
                    }
                    e->n = m;
                }
            }
            if (!e->valido) continue;
            fatos_copiar(&tmp, e);
            for (size_t k = g.ini[b]; k < g.fim[b]; k++) opt_transferir(&tmp, &u->ins[k], G);
            if (!fatos_igual(&tmp, &sai[b])) {
                fatos_copiar(&sai[b], &tmp);
                mudou = 1;
            }
        }
    }

    int alterou = 0;
    for (size_t b = 0; b < nb; b++) {
        fatos_copiar(&tmp, &ent[b]);
        for (size_t k = g.ini[b]; k < g.fim[b]; k++) {
            if (opt_substituir(&u->ins[k], opt_troca_fatos, &tmp)) alterou = 1;
            opt_transferir(&tmp, &u->ins[k], G);
        }
    }

    for (size_t b = 0; b < nb; b++) {
        fatos_liberar(&ent[b]);
        fatos_liberar(&sai[b]);
    }
    free(ent);
    free(sai);
    fatos_liberar(&tmp);
    grafo_liberar(&g);
    return alterou;
}

/* ---- subexpressoes comuns ------------------------------------------ */

typedef struct {
    IrOp op;
    const char *opr, *a, *b, *dono;
} OptExpr;

static OptExpr opt_chave(const Instr *i) {
    OptExpr e;
    e.op = i->op;
    e.opr = i->opr;
    e.a = i->a;
    e.b = i->op == I_BIN ? i->b : ir_str("");
    e.dono = NULL;
    if (i->op == I_BIN) {
        const char *op = i->opr;
        int comut = opt_eq(op, "+") || opt_eq(op, "*") || opt_eq(op, "==")
                    || opt_eq(op, "!=") || opt_eq(op, "and") || opt_eq(op, "or");
        if (comut && strcmp(e.b, e.a) < 0) {
            const char *t = e.a;
            e.a = e.b;
            e.b = t;
        }
    }
    return e;
}

static int opt_subexpressoes(Unidade *u, const OptConj *G) {
    OptGrafo g;
    grafo_montar(&g, u->ins, u->n);
    OptExpr *tab = NULL;
    size_t nt = 0, capt = 0;
    int mudou = 0;
    for (size_t b = 0; b < g.nb; b++) {
        nt = 0;
        for (size_t k = g.ini[b]; k < g.fim[b]; k++) {
            Instr *i = &u->ins[k];
            if (i->op == I_BIN || i->op == I_UN) {
                OptExpr c = opt_chave(i);
                for (size_t t = 0; t < nt; t++) {
                    if (tab[t].op == c.op && tab[t].opr == c.opr && tab[t].a == c.a
                        && tab[t].b == c.b) {
                        if (tab[t].dono != i->dst) {
                            const char *dst = i->dst, *dono = tab[t].dono;
                            memset(i, 0, sizeof(*i));
                            i->op = I_COPY;
                            i->dst = dst;
                            i->a = dono;
                            mudou = 1;
                        }
                        break;
                    }
                }
            }
            const char *d = opt_definida(i);
            if (d) {
                size_t m = 0;
                for (size_t t = 0; t < nt; t++) {
                    if (tab[t].dono == d || tab[t].a == d || tab[t].b == d) continue;
                    tab[m++] = tab[t];
                }
                nt = m;
            }
            if (i->op == I_CALL) {
                size_t m = 0;
                for (size_t t = 0; t < nt; t++) {
                    if (conj_tem(G, tab[t].dono) || conj_tem(G, tab[t].a)
                        || conj_tem(G, tab[t].b)) continue;
                    tab[m++] = tab[t];
                }
                nt = m;
            }
            if (i->op == I_BIN || i->op == I_UN) {
                OptExpr c = opt_chave(i);
                if (i->dst != c.a && i->dst != c.b) {
                    size_t t = 0;
                    while (t < nt && !(tab[t].op == c.op && tab[t].opr == c.opr
                                       && tab[t].a == c.a && tab[t].b == c.b)) t++;
                    if (t == nt) {
                        OPT_CRESCER(tab, nt, capt);
                        nt++;
                    }
                    c.dono = i->dst;
                    tab[t] = c;
                }
            }
        }
    }
    free(tab);
    grafo_liberar(&g);
    return mudou;
}

/* ---- codigo morto -------------------------------------------------- */

static void opt_usos_vivos(OptConj *vivas, const Instr *i, const OptConj *G) {
    const char *us[2];
    size_t n = opt_usos(i, us);
    for (size_t k = 0; k < n; k++) conj_add(vivas, us[k]);
    if (i->op == I_CALL) conj_uniao(vivas, G);
}

static int opt_removivel(IrOp op) {
    return op == I_COPY || op == I_BIN || op == I_UN || op == I_LOAD;
}

static int opt_codigo_morto(Unidade *u, const OptConj *G) {
    OptGrafo g;
    grafo_montar(&g, u->ins, u->n);
    size_t nb = g.nb;
    OptConj *ent = calloc(nb + 1, sizeof(OptConj));
    OptConj *sai = calloc(nb + 1, sizeof(OptConj));
    OptConj s = {0}, v = {0};

    int mudou = 1;
    while (mudou) {
        mudou = 0;
        for (size_t bb = nb; bb > 0; bb--) {
            size_t b = bb - 1;
            s.n = 0;
            if (g.nsuc[b] == 0) conj_uniao(&s, G);
            for (int k = 0; k < g.nsuc[b]; k++) conj_uniao(&s, &ent[g.suc[b][k]]);
            conj_copiar(&v, &s);
            for (size_t k = g.fim[b]; k > g.ini[b]; k--) {
                const Instr *i = &u->ins[k - 1];
                const char *d = opt_definida(i);
                if (d) conj_del(&v, d);
                opt_usos_vivos(&v, i, G);
            }
            if (!conj_igual(&s, &sai[b]) || !conj_igual(&v, &ent[b])) {
                conj_copiar(&sai[b], &s);
                conj_copiar(&ent[b], &v);
                mudou = 1;
            }
        }
    }

    int alterou = 0;
    char *remover = calloc(u->n + 1, 1);
    int algum = 0;
    for (size_t b = 0; b < nb; b++) {
        conj_copiar(&v, &sai[b]);
        for (size_t k = g.fim[b]; k > g.ini[b]; k--) {
            Instr *i = &u->ins[k - 1];
            const char *d = opt_definida(i);
            if (i->op == I_COPY && i->dst == i->a) {
                remover[k - 1] = 1;
                algum = 1;
                continue;
            }
            if (opt_removivel(i->op) && !conj_tem(&v, d)) {
                remover[k - 1] = 1;
                algum = 1;
                continue;
            }
            if (i->op == I_CALL && d && !conj_tem(&v, d)) {
                i->dst = NULL;
                d = NULL;
                alterou = 1;
            }
            if (d) conj_del(&v, d);
            opt_usos_vivos(&v, i, G);
        }
    }
    if (algum) {
        size_t m = 0;
        for (size_t k = 0; k < u->n; k++) {
            if (!remover[k]) u->ins[m++] = u->ins[k];
        }
        u->n = m;
        alterou = 1;
    }

    free(remover);
    for (size_t b = 0; b < nb; b++) {
        free(ent[b].v);
        free(sai[b].v);
    }
    free(ent);
    free(sai);
    free(s.v);
    free(v.v);
    grafo_liberar(&g);
    return alterou;
}

/* ---- contagem de usos/definicoes ----------------------------------- */

typedef struct {
    const char **nome;
    int *usos, *defs;
    size_t n, cap;
} OptContagem;

static size_t cont_idx(OptContagem *c, const char *x) {
    for (size_t i = 0; i < c->n; i++) {
        if (c->nome[i] == x) return i;
    }
    if (c->n == c->cap) {
        c->cap = c->cap ? c->cap * 2 : 16;
        c->nome = opt_xalloc(c->nome, c->cap * sizeof(char *));
        c->usos = opt_xalloc(c->usos, c->cap * sizeof(int));
        c->defs = opt_xalloc(c->defs, c->cap * sizeof(int));
    }
    c->nome[c->n] = x;
    c->usos[c->n] = c->defs[c->n] = 0;
    return c->n++;
}

static void cont_montar(OptContagem *c, const Instr *ins, size_t n) {
    memset(c, 0, sizeof(*c));
    for (size_t k = 0; k < n; k++) {
        const char *us[2];
        size_t nu = opt_usos(&ins[k], us);
        for (size_t j = 0; j < nu; j++) {
            size_t idx = cont_idx(c, us[j]);
            c->usos[idx]++;
        }
        const char *d = opt_definida(&ins[k]);
        if (d) {
            size_t idx = cont_idx(c, d);
            c->defs[idx]++;
        }
    }
}

static int cont_usos(OptContagem *c, const char *x) {
    size_t idx = cont_idx(c, x);
    return c->usos[idx];
}

static int cont_defs(OptContagem *c, const char *x) {
    size_t idx = cont_idx(c, x);
    return c->defs[idx];
}

static void cont_liberar(OptContagem *c) {
    free(c->nome);
    free(c->usos);
    free(c->defs);
}

/* t = a + b; x = t   ->   x = a + b   (t temporario usado so ali) */
static int opt_juntar_copias(Unidade *u, const OptConj *G) {
    (void) G;
    OptContagem c;
    cont_montar(&c, u->ins, u->n);
    int mudou = 0;
    size_t k = 0;
    while (k + 1 < u->n) {
        Instr *i = &u->ins[k], *j = &u->ins[k + 1];
        const char *t = opt_definida(i);
        int forma = i->op == I_COPY || i->op == I_BIN || i->op == I_UN
                    || i->op == I_LOAD || i->op == I_CALL;
        if (forma && t && ir_eh_temp(t) && j->op == I_COPY && j->a == t
            && j->dst != t && cont_usos(&c, t) == 1 && cont_defs(&c, t) == 1) {
            i->dst = j->dst;
            memmove(&u->ins[k + 1], &u->ins[k + 2], (u->n - k - 2) * sizeof(Instr));
            u->n--;
            mudou = 1;
        }
        k++;
    }
    cont_liberar(&c);
    return mudou;
}

/* ---- fluxo de controle --------------------------------------------- */

static int opt_limpar_fluxo(Unidade *u, const OptConj *G) {
    (void) G;
    int mudou = 0;
    OptGrafo g;
    grafo_montar(&g, u->ins, u->n);
    if (g.nalc < g.nb) {
        size_t m = 0;
        for (size_t b = 0; b < g.nb; b++) {
            if (!g.alc[b]) continue;
            for (size_t k = g.ini[b]; k < g.fim[b]; k++) u->ins[m++] = u->ins[k];
        }
        u->n = m;
        mudou = 1;
    }
    grafo_liberar(&g);

    char *tirar = calloc(u->n + 1, 1);
    size_t restantes = u->n;
    for (size_t k = 0; k < u->n; k++) {
        if (!opt_salto(u->ins[k].op)) continue;
        for (size_t j = k + 1; j < u->n && u->ins[j].op == I_LABEL; j++) {
            if (u->ins[j].opr == u->ins[k].opr) {
                tirar[k] = 1;
                restantes--;
                mudou = 1;
                break;
            }
        }
    }
    OptConj alvos = {0};
    for (size_t k = 0; k < u->n; k++) {
        if (!tirar[k] && opt_salto(u->ins[k].op)) conj_add(&alvos, u->ins[k].opr);
    }
    size_t m = 0;
    for (size_t k = 0; k < u->n; k++) {
        if (tirar[k]) continue;
        if (u->ins[k].op == I_LABEL && !conj_tem(&alvos, u->ins[k].opr)) continue;
        u->ins[m++] = u->ins[k];
    }
    if (m != restantes) mudou = 1;
    u->n = m;
    free(tirar);
    free(alvos.v);
    return mudou;
}

/* ---- movimento de codigo invariante -------------------------------- */

static int opt_mover_invariantes(Unidade *u, const OptConj *G) {
    OptGrafo g;
    grafo_montar(&g, u->ins, u->n);
    size_t nb = g.nb;
    int moveu = 0;
    if (nb == 0) {
        grafo_liberar(&g);
        return 0;
    }

    /* dom[b*nb + x]: x domina b */
    char *dom = opt_xalloc(NULL, nb * nb);
    for (size_t b = 0; b < nb; b++) {
        for (size_t x = 0; x < nb; x++) dom[b * nb + x] = g.alc[x];
    }
    for (size_t x = 0; x < nb; x++) dom[x] = x == 0;
    char *novo = opt_xalloc(NULL, nb);
    int mudou = 1;
    while (mudou) {
        mudou = 0;
        for (size_t b = 1; b < nb; b++) {
            if (!g.alc[b]) continue;
            for (size_t x = 0; x < nb; x++) novo[x] = g.alc[x];
            for (int k = 0; k < g.npred[b]; k++) {
                int p = g.pred[b][k];
                if (!g.alc[p]) continue;
                for (size_t x = 0; x < nb; x++) novo[x] &= dom[(size_t) p * nb + x];
            }
            novo[b] = 1;
            if (memcmp(novo, &dom[b * nb], nb) != 0) {
                memcpy(&dom[b * nb], novo, nb);
                mudou = 1;
            }
        }
    }

    /* corpo[h*nb + m]: m pertence ao laco de cabecalho h */
    char *corpo = calloc(nb * nb, 1);
    char *eh_laco = calloc(nb, 1);
    int *pilha = opt_xalloc(NULL, (nb * nb + nb + 1) * sizeof(int));
    for (size_t b = 0; b < nb; b++) {
        if (!g.alc[b]) continue;
        for (int s = 0; s < g.nsuc[b]; s++) {
            size_t h = (size_t) g.suc[b][s];
            if (!dom[b * nb + h]) continue;
            char *c = &corpo[h * nb];
            eh_laco[h] = 1;
            c[h] = 1;
            size_t np = 0;
            pilha[np++] = (int) b;
            while (np) {
                int m = pilha[--np];
                if (c[m]) continue;
                c[m] = 1;
                for (int k = 0; k < g.npred[m]; k++) {
                    if (g.alc[g.pred[m][k]]) pilha[np++] = g.pred[m][k];
                }
            }
        }
    }

    OptContagem cont;
    cont_montar(&cont, u->ins, u->n);
    char *no_laco = calloc(u->n + 1, 1);
    size_t *movidas = opt_xalloc(NULL, (u->n + 1) * sizeof(size_t));
    char *movida = calloc(u->n + 1, 1);
    OptConj defs_laco = {0}, usos_fora = {0}, movidos = {0};

    for (size_t h = 1; h < nb && !moveu; h++) {
        if (!eh_laco[h]) continue;
        const char *c = &corpo[h * nb];
        int nfora = 0, unico = -1;
        for (int k = 0; k < g.npred[h]; k++) {
            int p = g.pred[h][k];
            if (!c[p] && g.alc[p]) {
                nfora++;
                unico = p;
            }
        }
        if (nfora != 1 || unico != (int) h - 1) continue;
        IrOp ult = u->ins[g.fim[h - 1] - 1].op;
        if (opt_salto(ult) || ult == I_RETURN) continue;

        memset(no_laco, 0, u->n);
        defs_laco.n = usos_fora.n = movidos.n = 0;
        int tem_chamada = 0;
        for (size_t b = 0; b < nb; b++) {
            if (!c[b]) continue;
            for (size_t k = g.ini[b]; k < g.fim[b]; k++) {
                no_laco[k] = 1;
                conj_add(&defs_laco, opt_definida(&u->ins[k]));
                if (u->ins[k].op == I_CALL) tem_chamada = 1;
            }
        }
        for (size_t k = 0; k < u->n; k++) {
            if (no_laco[k]) continue;
            const char *us[2];
            size_t nu = opt_usos(&u->ins[k], us);
            for (size_t j = 0; j < nu; j++) conj_add(&usos_fora, us[j]);
        }

        size_t nmov = 0;
        memset(movida, 0, u->n);
        int progresso = 1;
        while (progresso) {
            progresso = 0;
            for (size_t b = 0; b < nb; b++) {
                if (!c[b]) continue;
                for (size_t k = g.ini[b]; k < g.fim[b]; k++) {
                    const Instr *i = &u->ins[k];
                    if (movida[k] || (i->op != I_BIN && i->op != I_UN)) continue;
                    if (i->op == I_BIN && (opt_eq(i->opr, "/") || opt_eq(i->opr, "%"))) continue;
                    const char *d = i->dst;
                    if (!ir_eh_temp(d) || cont_defs(&cont, d) != 1 || conj_tem(&usos_fora, d)) continue;
                    const char *ops[2] = {i->a, i->op == I_BIN ? i->b : NULL};
                    int ok = 1;
                    for (int j = 0; j < 2 && ok; j++) {
                        const char *x = ops[j];
                        if (!x) continue;
                        if (opt_eh_const(x) || conj_tem(&movidos, x)) continue;
                        if (conj_tem(&defs_laco, x) || (tem_chamada && conj_tem(G, x))) ok = 0;
                    }
                    if (ok) {
                        movida[k] = 1;
                        movidas[nmov++] = k;
                        conj_add(&movidos, d);
                        progresso = 1;
                    }
                }
            }
        }
        if (nmov) {
            size_t antes = g.ini[h];
            Instr *novas = opt_xalloc(NULL, (u->n + 1) * sizeof(Instr));
            size_t m = 0;
            for (size_t k = 0; k < u->n; k++) {
                if (k == antes) {
                    for (size_t j = 0; j < nmov; j++) novas[m++] = u->ins[movidas[j]];
                }
                if (!movida[k]) novas[m++] = u->ins[k];
            }
            memcpy(u->ins, novas, m * sizeof(Instr));
            u->n = m;
            free(novas);
            moveu = 1;
        }
    }

    free(defs_laco.v);
    free(usos_fora.v);
    free(movidos.v);
    free(movida);
    free(movidas);
    free(no_laco);
    cont_liberar(&cont);
    free(pilha);
    free(eh_laco);
    free(corpo);
    free(novo);
    free(dom);
    grafo_liberar(&g);
    return moveu;
}

/* ---- laco principal ------------------------------------------------ */

typedef int (*OptPasso)(Unidade *u, const OptConj *G);

static const OptPasso OPT_PASSOS[] = {
    opt_simplificar, opt_propagar, opt_subexpressoes, opt_codigo_morto,
    opt_juntar_copias, opt_limpar_fluxo, opt_mover_invariantes,
};

static void opt_unidade(Unidade *u, const OptConj *G) {
    for (int it = 0; it < 100; it++) {
        int mudou = 0;
        for (size_t p = 0; p < sizeof(OPT_PASSOS) / sizeof(OPT_PASSOS[0]); p++) {
            if (OPT_PASSOS[p](u, G)) mudou = 1;
        }
        if (!mudou) break;
    }
}

void opt_otimizar(ProgramaIR *p) {
    OptConj G = {0};
    for (size_t k = 0; k < p->nglobais; k++) conj_add(&G, p->globais[k]);
    opt_unidade(&p->init, &G);
    for (size_t f = 0; f < p->nfuncoes; f++) opt_unidade(&p->funcoes[f], &G);
    free(G.v);
}
