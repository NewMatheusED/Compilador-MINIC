#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "semantic.h"

static void *xmalloc(size_t n) {
    void *p = malloc(n ? n : 1);
    if (!p) {
        fprintf(stderr, "erro: memoria insuficiente\n");
        exit(1);
    }
    return p;
}

static void *xrealloc(void *p, size_t n) {
    p = realloc(p, n ? n : 1);
    if (!p) {
        fprintf(stderr, "erro: memoria insuficiente\n");
        exit(1);
    }
    return p;
}

typedef struct {
    char *s;
    size_t len;
    size_t cap;
} Sb;

static void sb_reserve(Sb *sb, size_t extra) {
    if (sb->len + extra + 1 > sb->cap) {
        size_t nova = sb->cap ? sb->cap : 64;
        while (sb->len + extra + 1 > nova) nova *= 2;
        sb->s = (char *) xrealloc(sb->s, nova);
        sb->cap = nova;
    }
}

static void sb_puts(Sb *sb, const char *txt) {
    size_t n = strlen(txt);
    sb_reserve(sb, n);
    memcpy(sb->s + sb->len, txt, n + 1);
    sb->len += n;
}

static void sb_vprintf(Sb *sb, const char *fmt, va_list ap) {
    va_list copia;
    va_copy(copia, ap);
    int n = vsnprintf(NULL, 0, fmt, copia);
    va_end(copia);
    if (n < 0) return;
    sb_reserve(sb, (size_t) n);
    vsnprintf(sb->s + sb->len, (size_t) n + 1, fmt, ap);
    sb->len += (size_t) n;
}

static void sb_printf(Sb *sb, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    sb_vprintf(sb, fmt, ap);
    va_end(ap);
}

static char *sb_take(Sb *sb) {
    if (!sb->s) {
        char *vazio = (char *) xmalloc(1);
        vazio[0] = '\0';
        return vazio;
    }
    char *s = sb->s;
    sb->s = NULL;
    sb->len = sb->cap = 0;
    return s;
}

#define ABRE_ASPAS  "“"
#define FECHA_ASPAS "”"
#define TRAVESSAO   "—"

/* os .gabarito do professor usam CRLF e nao tem \n no final */
#define SEPARADOR "\r\n"

typedef enum {
    TB_INT, TB_FLOAT, TB_BOOL, TB_CHAR, TB_VOID, TB_STRING, TB_ERRO
} TipoBase;

typedef struct {
    TipoBase base;
    int vetor;
} Tipo;

static const Tipo T_INT   = {TB_INT, 0};
static const Tipo T_FLOAT = {TB_FLOAT, 0};
static const Tipo T_BOOL  = {TB_BOOL, 0};
static const Tipo T_CHAR  = {TB_CHAR, 0};
static const Tipo T_VOID  = {TB_VOID, 0};
static const Tipo T_STRING = {TB_STRING, 0};
static const Tipo T_ERRO  = {TB_ERRO, 0};

static const char *NOMES_TIPO[][2] = {
    {"int", "int[]"},       {"float", "float[]"},   {"bool", "bool[]"},
    {"char", "char[]"},     {"void", "void[]"},     {"string", "string[]"},
    {"erro", "erro[]"},
};

static const char *ts(Tipo t) { return NOMES_TIPO[t.base][t.vetor ? 1 : 0]; }

static int tipo_eq(Tipo a, Tipo b) {
    return a.base == b.base && (a.vetor != 0) == (b.vetor != 0);
}

static int eh_erro(Tipo t) { return t.base == TB_ERRO; }

static int numerico(Tipo t) {
    return !t.vetor && (t.base == TB_INT || t.base == TB_FLOAT
                        || t.base == TB_CHAR);
}

static int inteiro(Tipo t) {
    return !t.vetor && (t.base == TB_INT || t.base == TB_CHAR);
}

static TipoBase base_de_nome(const char *nome) {
    if (strcmp(nome, "int") == 0)   return TB_INT;
    if (strcmp(nome, "float") == 0) return TB_FLOAT;
    if (strcmp(nome, "bool") == 0)  return TB_BOOL;
    if (strcmp(nome, "char") == 0)  return TB_CHAR;
    if (strcmp(nome, "void") == 0)  return TB_VOID;
    return TB_ERRO;
}

static Tipo tipo_de(const char *nome, int vetor) {
    Tipo t;
    t.base = base_de_nome(nome);
    t.vetor = vetor;
    return t;
}

static Tipo tipo_literal(const char *categoria) {
    if (strcmp(categoria, "int") == 0)  return T_INT;
    if (strcmp(categoria, "real") == 0) return T_FLOAT;
    if (strcmp(categoria, "bool") == 0) return T_BOOL;
    if (strcmp(categoria, "char") == 0) return T_CHAR;
    return T_STRING;
}

static const char *nome_literal(const char *categoria) {
    if (strcmp(categoria, "int") == 0)  return "inteiro";
    if (strcmp(categoria, "real") == 0) return "real";
    if (strcmp(categoria, "bool") == 0) return "booleano";
    if (strcmp(categoria, "char") == 0) return "caractere";
    return "string";
}

/* promocoes permitidas: int -> float, char -> int, char -> float */
static int compativel(Tipo destino, Tipo origem) {
    if (eh_erro(destino) || eh_erro(origem)) return 1;
    if (destino.vetor || origem.vetor) return tipo_eq(destino, origem);
    if (destino.base == origem.base) return 1;
    if (destino.base == TB_FLOAT
        && (origem.base == TB_INT || origem.base == TB_CHAR)) return 1;
    if (destino.base == TB_INT && origem.base == TB_CHAR) return 1;
    return 0;
}

typedef enum { CAT_VARIAVEL, CAT_PARAMETRO, CAT_VETOR, CAT_FUNCAO } Categoria;

typedef struct {
    const char *nome;
    Categoria categoria;
    Tipo tipo;               /* na funcao, e o tipo de retorno */
    int linha, coluna;
    Tipo *params;
    size_t nparams;
} Simbolo;

typedef struct {
    Simbolo **itens;
    size_t n, cap;
} ListaSimbolos;

static void lista_add(ListaSimbolos *l, Simbolo *s) {
    if (l->n == l->cap) {
        l->cap = l->cap ? l->cap * 2 : 8;
        l->itens = (Simbolo **) xrealloc(l->itens, l->cap * sizeof(Simbolo *));
    }
    l->itens[l->n++] = s;
}

static Simbolo *lista_buscar(const ListaSimbolos *l, const char *nome) {
    for (size_t i = 0; i < l->n; i++) {
        if (strcmp(l->itens[i]->nome, nome) == 0) return l->itens[i];
    }
    return NULL;
}

static int eh_funcao(const Simbolo *s) { return s->categoria == CAT_FUNCAO; }

static void descrever(const Simbolo *s, char *buf, size_t tam) {
    snprintf(buf, tam, "%s %s", eh_funcao(s) ? "função" : "tipo",
             ts(s->tipo));
}

typedef struct {
    char codigo[8];
    int linha, coluna;
    char *mensagem;
    size_t ordem;
} Diagnostico;

typedef struct {
    ListaSimbolos *escopos;
    size_t nescopos, capescopos;

    ListaSimbolos todos;         /* pra liberar tudo no final */

    Diagnostico *diags;
    size_t ndiags, capdiags;

    char **textos;
    size_t ntextos, captextos;

    const AstNode *funcao_atual;
} Analisador;

static Simbolo *novo_simbolo(Analisador *an, const char *nome, Categoria cat,
                             Tipo tipo, int linha, int coluna) {
    Simbolo *s = (Simbolo *) xmalloc(sizeof(Simbolo));
    memset(s, 0, sizeof(*s));
    s->nome = nome;
    s->categoria = cat;
    s->tipo = tipo;
    s->linha = linha;
    s->coluna = coluna;
    lista_add(&an->todos, s);
    return s;
}

static void abrir_escopo(Analisador *an) {
    if (an->nescopos == an->capescopos) {
        an->capescopos = an->capescopos ? an->capescopos * 2 : 8;
        an->escopos = (ListaSimbolos *) xrealloc(
            an->escopos, an->capescopos * sizeof(ListaSimbolos));
    }
    memset(&an->escopos[an->nescopos], 0, sizeof(ListaSimbolos));
    an->nescopos++;
}

static void fechar_escopo(Analisador *an) {
    an->nescopos--;
    free(an->escopos[an->nescopos].itens);
}

static Simbolo *buscar_no_escopo_atual(Analisador *an, const char *nome) {
    return lista_buscar(&an->escopos[an->nescopos - 1], nome);
}

static Simbolo *buscar(Analisador *an, const char *nome) {
    for (size_t i = an->nescopos; i > 0; i--) {
        Simbolo *s = lista_buscar(&an->escopos[i - 1], nome);
        if (s) return s;
    }
    return NULL;
}

static void inserir(Analisador *an, Simbolo *s) {
    lista_add(&an->escopos[an->nescopos - 1], s);
}

static const char *guardar(Analisador *an, char *s) {
    if (an->ntextos == an->captextos) {
        an->captextos = an->captextos ? an->captextos * 2 : 16;
        an->textos = (char **) xrealloc(an->textos,
                                        an->captextos * sizeof(char *));
    }
    an->textos[an->ntextos++] = s;
    return s;
}

static void diag(Analisador *an, const char *codigo, int linha, int coluna,
                 const char *fmt, ...) {
    if (an->ndiags == an->capdiags) {
        an->capdiags = an->capdiags ? an->capdiags * 2 : 8;
        an->diags = (Diagnostico *) xrealloc(an->diags,
                                             an->capdiags * sizeof(Diagnostico));
    }
    Diagnostico *d = &an->diags[an->ndiags];
    snprintf(d->codigo, sizeof(d->codigo), "%s", codigo);
    d->linha = linha;
    d->coluna = coluna;
    d->ordem = an->ndiags;

    Sb sb = {0};
    va_list ap;
    va_start(ap, fmt);
    sb_vprintf(&sb, fmt, ap);
    va_end(ap);
    d->mensagem = sb_take(&sb);
    an->ndiags++;
}

#define ERRO_NO(an, cod, no, ...) \
    diag((an), (cod), (no)->linha, (no)->coluna, __VA_ARGS__)

#define PREC_ATRIB   1
#define PREC_UNARIA  8
#define PREC_POSFIXA 9

static int prec_op(const char *op) {
    if (strcmp(op, "||") == 0) return 2;
    if (strcmp(op, "&&") == 0) return 3;
    if (strcmp(op, "==") == 0 || strcmp(op, "!=") == 0) return 4;
    if (strcmp(op, "<") == 0 || strcmp(op, "<=") == 0
        || strcmp(op, ">") == 0 || strcmp(op, ">=") == 0) return 5;
    if (strcmp(op, "+") == 0 || strcmp(op, "-") == 0) return 6;
    return 7; /* * / % */
}

static int prec_no(const AstNode *no) {
    switch (no->kind) {
        case N_ASSIGN: return PREC_ATRIB;
        case N_BINARY: return prec_op(no->a);
        case N_UNARY:  return PREC_UNARIA;
        default:       return PREC_POSFIXA;
    }
}

static void texto_sb(Sb *sb, const AstNode *no, int minimo) {
    int parens = prec_no(no) < minimo;
    if (parens) sb_puts(sb, "(");

    switch (no->kind) {
        case N_ID:
            sb_puts(sb, no->a);
            break;
        case N_LIT:
            sb_puts(sb, no->b);
            break;
        case N_BINARY: {
            int p = prec_op(no->a);
            texto_sb(sb, no->kids[0], p);
            sb_printf(sb, " %s ", no->a);
            texto_sb(sb, no->kids[1], p + 1);
            break;
        }
        case N_UNARY:
            sb_puts(sb, no->a);
            texto_sb(sb, no->kids[0], PREC_UNARIA);
            break;
        case N_ASSIGN:
            texto_sb(sb, no->kids[0], PREC_ATRIB + 1);
            sb_puts(sb, " = ");
            texto_sb(sb, no->kids[1], PREC_ATRIB);
            break;
        case N_CALL:
            texto_sb(sb, no->kids[0], PREC_POSFIXA);
            sb_puts(sb, "(");
            for (size_t i = 1; i < no->nkids; i++) {
                if (i > 1) sb_puts(sb, ", ");
                texto_sb(sb, no->kids[i], 0);
            }
            sb_puts(sb, ")");
            break;
        case N_INDEX:
            texto_sb(sb, no->kids[0], PREC_POSFIXA);
            sb_puts(sb, "[");
            texto_sb(sb, no->kids[1], 0);
            sb_puts(sb, "]");
            break;
        default:
            sb_puts(sb, "?");
            break;
    }

    if (parens) sb_puts(sb, ")");
}

static const char *texto(Analisador *an, const AstNode *no) {
    Sb sb = {0};
    texto_sb(&sb, no, 0);
    return guardar(an, sb_take(&sb));
}

static void erro_duplicado(Analisador *an, const Simbolo *novo,
                           const Simbolo *anterior) {
    char desc[64];
    descrever(anterior, desc, sizeof(desc));
    diag(an, "SEM002", novo->linha, novo->coluna,
         ABRE_ASPAS "%s" FECHA_ASPAS " já declarado neste escopo; "
         "declaração anterior na linha %d, coluna %d (%s).",
         novo->nome, anterior->linha, anterior->coluna, desc);
}

static void declarar(Analisador *an, Simbolo *s) {
    Simbolo *anterior = buscar_no_escopo_atual(an, s->nome);
    if (anterior) {
        erro_duplicado(an, s, anterior);
        return;
    }
    inserir(an, s);
}

static Simbolo *assinatura(Analisador *an, const AstNode *f) {
    Simbolo *s = novo_simbolo(an, f->b, CAT_FUNCAO, tipo_de(f->a, 0),
                              f->linha2, f->coluna2);
    s->nparams = f->nparams;
    s->params = (Tipo *) xmalloc(f->nparams * sizeof(Tipo));
    for (size_t i = 0; i < f->nparams; i++) {
        const AstNode *p = f->kids[i];
        s->params[i] = tipo_de(p->a, p->vetor);
    }
    return s;
}

static Simbolo *simbolo_var(Analisador *an, const AstNode *d) {
    int vetor = d->kids[0] != NULL;
    return novo_simbolo(an, d->b, vetor ? CAT_VETOR : CAT_VARIAVEL,
                        tipo_de(d->a, vetor), d->linha, d->coluna);
}

static Tipo expr(Analisador *an, const AstNode *no);
static Tipo indexacao(Analisador *an, const AstNode *no);

static Tipo ident(Analisador *an, const AstNode *no) {
    Simbolo *s = buscar(an, no->a);
    if (!s) {
        ERRO_NO(an, "SEM001", no,
                "Identificador " ABRE_ASPAS "%s" FECHA_ASPAS
                " não declarado neste escopo.", no->a);
        return T_ERRO;
    }
    if (eh_funcao(s)) {
        ERRO_NO(an, "SEM014", no,
                "Função " ABRE_ASPAS "%s" FECHA_ASPAS
                " usada como valor; faltam os parênteses da chamada.",
                no->a);
        return T_ERRO;
    }
    return s->tipo;
}

static Tipo valor(Analisador *an, const AstNode *no, const char *contexto) {
    Tipo t = expr(an, no);
    if (tipo_eq(t, T_VOID) && no->kind == N_CALL && no->kids[0]->kind == N_ID) {
        ERRO_NO(an, "SEM012", no,
                "Função " ABRE_ASPAS "%s" FECHA_ASPAS
                " não produz valor (retorno void) e não pode ser "
                "usada como %s.", no->kids[0]->a, contexto);
        return T_ERRO;
    }
    if (tipo_eq(t, T_VOID)) return T_ERRO;
    return t;
}

static Tipo destino(Analisador *an, const AstNode *no) {
    if (no->kind == N_ID) {
        Simbolo *s = buscar(an, no->a);
        if (!s) {
            ident(an, no);
            return T_ERRO;
        }
        if (eh_funcao(s)) {
            ERRO_NO(an, "SEM013", no,
                    "Destino de atribuição não é "
                    "atribuível; a função "
                    ABRE_ASPAS "%s" FECHA_ASPAS " não designa uma "
                    "variável ou elemento de vetor.", no->a);
            return T_ERRO;
        }
        if (s->tipo.vetor) {
            ERRO_NO(an, "SEM013", no,
                    "Destino de atribuição não é "
                    "atribuível; o vetor " ABRE_ASPAS "%s" FECHA_ASPAS
                    " não pode ser atribuído como um todo, "
                    "apenas seus elementos.", no->a);
            return T_ERRO;
        }
        return s->tipo;
    }
    if (no->kind == N_INDEX) return indexacao(an, no);

    char o_que[512];
    if (no->kind == N_LIT) {
        snprintf(o_que, sizeof(o_que), "o literal %s " ABRE_ASPAS "%s"
                 FECHA_ASPAS, nome_literal(no->a), no->b);
    } else if (no->kind == N_CALL) {
        snprintf(o_que, sizeof(o_que), "a chamada " ABRE_ASPAS "%s"
                 FECHA_ASPAS, texto(an, no));
    } else if (no->kind == N_ASSIGN) {
        snprintf(o_que, sizeof(o_que), "a atribuição "
                 ABRE_ASPAS "%s" FECHA_ASPAS, texto(an, no));
    } else {
        snprintf(o_que, sizeof(o_que), "a expressão " ABRE_ASPAS "%s"
                 FECHA_ASPAS, texto(an, no));
    }
    expr(an, no);
    ERRO_NO(an, "SEM013", no,
            "Destino de atribuição não é "
            "atribuível; %s não designa uma variável ou "
            "elemento de vetor.", o_que);
    return T_ERRO;
}

static Tipo atribuicao(Analisador *an, const AstNode *no) {
    Tipo d = destino(an, no->kids[0]);
    Tipo t = valor(an, no->kids[1], "expressão de atribuição");
    if (eh_erro(d)) return T_ERRO;
    if (!compativel(d, t)) {
        ERRO_NO(an, "SEM003", no->kids[1],
                "Não é possível atribuir %s a %s sem "
                "conversão permitida (destino " ABRE_ASPAS "%s"
                FECHA_ASPAS "; expressão " ABRE_ASPAS "%s" FECHA_ASPAS
                ").", ts(t), ts(d), texto(an, no->kids[0]),
                texto(an, no->kids[1]));
    }
    return d;
}

static void operando_invalido(Analisador *an, const AstNode *no, const char *op,
                              int linha, int coluna, const char *tipos) {
    diag(an, "SEM004", linha, coluna,
         "Operador " ABRE_ASPAS "%s" FECHA_ASPAS " não se aplica a %s "
         "(expressão " ABRE_ASPAS "%s" FECHA_ASPAS ").",
         op, tipos, texto(an, no));
}

static void divisao_por_zero(Analisador *an, const AstNode *no) {
    if (strcmp(no->a, "/") != 0 && strcmp(no->a, "%") != 0) return;
    const AstNode *d = no->kids[1];
    if (d->kind != N_LIT) return;
    if (strcmp(d->a, "int") != 0 && strcmp(d->a, "real") != 0) return;
    char *fim = NULL;
    double v = strtod(d->b, &fim);
    if (fim == d->b || *fim != '\0' || v != 0.0) return;
    ERRO_NO(an, "SEM015", d,
            "Divisor constante zero em " ABRE_ASPAS "%s" FECHA_ASPAS ".",
            texto(an, no));
}

static Tipo binaria(Analisador *an, const AstNode *no) {
    Tipo te = valor(an, no->kids[0], "operando");
    Tipo td = valor(an, no->kids[1], "operando");
    const char *op = no->a;
    int p = prec_op(op);
    int l = no->linha2, c = no->coluna2;

    if (eh_erro(te) || eh_erro(td)) {
        return p <= 5 ? T_BOOL : T_ERRO;
    }

    char tipos[64];
    snprintf(tipos, sizeof(tipos), "%s e %s", ts(te), ts(td));

    if (p == 6 || strcmp(op, "*") == 0 || strcmp(op, "/") == 0) {
        if (!(numerico(te) && numerico(td))) {
            operando_invalido(an, no, op, l, c, tipos);
            return T_ERRO;
        }
        divisao_por_zero(an, no);
        return (tipo_eq(te, T_FLOAT) || tipo_eq(td, T_FLOAT)) ? T_FLOAT : T_INT;
    }
    if (strcmp(op, "%") == 0) {
        if (!(inteiro(te) && inteiro(td))) {
            operando_invalido(an, no, op, l, c, tipos);
            return T_ERRO;
        }
        divisao_por_zero(an, no);
        return T_INT;
    }
    if (p == 5) {
        if (!(numerico(te) && numerico(td))) {
            operando_invalido(an, no, op, l, c, tipos);
        }
        return T_BOOL;
    }
    if (p == 4) {
        int ok = (numerico(te) && numerico(td))
                 || (tipo_eq(te, td) && tipo_eq(te, T_BOOL));
        if (!ok) operando_invalido(an, no, op, l, c, tipos);
        return T_BOOL;
    }
    if (!tipo_eq(te, T_BOOL) || !tipo_eq(td, T_BOOL)) {
        operando_invalido(an, no, op, l, c, tipos);
    }
    return T_BOOL;
}

static Tipo unaria(Analisador *an, const AstNode *no) {
    Tipo t = valor(an, no->kids[0], "operando");
    int negacao = strcmp(no->a, "!") == 0;
    if (eh_erro(t)) return negacao ? T_BOOL : T_ERRO;
    if (negacao) {
        if (!tipo_eq(t, T_BOOL)) {
            operando_invalido(an, no, "!", no->linha, no->coluna, ts(t));
        }
        return T_BOOL;
    }
    if (!numerico(t)) {
        operando_invalido(an, no, no->a, no->linha, no->coluna, ts(t));
        return T_ERRO;
    }
    return t.base == TB_CHAR ? T_INT : t;
}

static Tipo chamada(Analisador *an, const AstNode *no) {
    const AstNode *alvo = no->kids[0];
    size_t nargs = no->nkids - 1;

    if (alvo->kind != N_ID) {
        expr(an, alvo);
        for (size_t i = 0; i < nargs; i++) expr(an, no->kids[i + 1]);
        ERRO_NO(an, "SEM014", no,
                "A expressão " ABRE_ASPAS "%s" FECHA_ASPAS " não "
                "é uma função e não pode ser "
                "chamada.", texto(an, alvo));
        return T_ERRO;
    }

    const char *nome = alvo->a;
    Simbolo *s = buscar(an, nome);
    if (!s || !eh_funcao(s)) {
        for (size_t i = 0; i < nargs; i++) expr(an, no->kids[i + 1]);
        if (!s) {
            ERRO_NO(an, "SEM001", alvo,
                    "Identificador " ABRE_ASPAS "%s" FECHA_ASPAS
                    " não declarado neste escopo.", nome);
        } else {
            char desc[64];
            descrever(s, desc, sizeof(desc));
            ERRO_NO(an, "SEM014", alvo,
                    ABRE_ASPAS "%s" FECHA_ASPAS " não é uma "
                    "função (%s) e não pode ser chamado.",
                    nome, desc);
        }
        return T_ERRO;
    }

    Tipo *tipos = (Tipo *) xmalloc(nargs * sizeof(Tipo));
    for (size_t i = 0; i < nargs; i++) {
        tipos[i] = valor(an, no->kids[i + 1], "argumento");
    }

    if (s->nparams != nargs) {
        ERRO_NO(an, "SEM007", no,
                ABRE_ASPAS "%s" FECHA_ASPAS " espera %lu %s, mas recebeu %lu.",
                nome, (unsigned long) s->nparams,
                s->nparams == 1 ? "argumento" : "argumentos",
                (unsigned long) nargs);
    } else {
        for (size_t i = 0; i < nargs; i++) {
            if (!compativel(s->params[i], tipos[i])) {
                const AstNode *arg = no->kids[i + 1];
                ERRO_NO(an, "SEM008", arg,
                        "Argumento %lu de " ABRE_ASPAS "%s" FECHA_ASPAS
                        ": esperado %s, recebido %s (expressão "
                        ABRE_ASPAS "%s" FECHA_ASPAS ").",
                        (unsigned long) (i + 1), nome, ts(s->params[i]),
                        ts(tipos[i]),
                        texto(an, arg));
            }
        }
    }
    free(tipos);
    return s->tipo;
}

static Tipo indexacao(Analisador *an, const AstNode *no) {
    Tipo tb = expr(an, no->kids[0]);
    Tipo ti = valor(an, no->kids[1], "índice");
    const char *nome = texto(an, no->kids[0]);

    if (!eh_erro(tb) && !tb.vetor) {
        ERRO_NO(an, "SEM006", no->kids[0],
                ABRE_ASPAS "%s" FECHA_ASPAS " não é um vetor (%s) "
                "e não pode ser indexado.", nome, ts(tb));
        tb = T_ERRO;
    }
    if (!eh_erro(ti) && !inteiro(ti)) {
        ERRO_NO(an, "SEM006", no->kids[1],
                "Índice do vetor " ABRE_ASPAS "%s" FECHA_ASPAS
                " deve ser int; recebeu %s (expressão " ABRE_ASPAS "%s"
                FECHA_ASPAS ").", nome, ts(ti), texto(an, no->kids[1]));
    }
    if (eh_erro(tb)) return T_ERRO;
    Tipo elem = {tb.base, 0};
    return elem;
}

static Tipo expr(Analisador *an, const AstNode *no) {
    switch (no->kind) {
        case N_LIT:    return tipo_literal(no->a);
        case N_ID:     return ident(an, no);
        case N_ASSIGN: return atribuicao(an, no);
        case N_BINARY: return binaria(an, no);
        case N_UNARY:  return unaria(an, no);
        case N_CALL:   return chamada(an, no);
        case N_INDEX:  return indexacao(an, no);
        default:       return T_ERRO;
    }
}

static void comando(Analisador *an, const AstNode *cmd);

static int so_digitos_zero(const char *s) {
    if (!*s) return 0;
    for (; *s; s++) {
        if (*s != '0') return 0;
    }
    return 1;
}

static void decl_var(Analisador *an, const AstNode *d) {
    Simbolo *s = simbolo_var(an, d);
    const AstNode *tamanho = d->kids[0];
    const AstNode *init = d->kids[1];

    if (strcmp(d->a, "void") == 0) {
        ERRO_NO(an, "SEM014", d,
                "Variável " ABRE_ASPAS "%s" FECHA_ASPAS " não pode "
                "ter tipo void.", d->b);
        s->tipo.base = TB_ERRO;
    }

    if (tamanho) {
        Tipo t = valor(an, tamanho, "tamanho de vetor");
        if (!eh_erro(t) && !inteiro(t)) {
            ERRO_NO(an, "SEM006", tamanho,
                    "Tamanho do vetor " ABRE_ASPAS "%s" FECHA_ASPAS " deve ser "
                    "int; recebeu %s (expressão " ABRE_ASPAS "%s"
                    FECHA_ASPAS ").", d->b, ts(t), texto(an, tamanho));
        } else if (tamanho->kind == N_LIT && strcmp(tamanho->a, "int") == 0
                   && so_digitos_zero(tamanho->b)) {
            ERRO_NO(an, "SEM006", tamanho,
                    "Tamanho do vetor " ABRE_ASPAS "%s" FECHA_ASPAS " deve ser "
                    "positivo; recebeu %s.", d->b, tamanho->b);
        }
    }

    if (init) {
        Tipo t = valor(an, init, "valor de inicialização");
        if (!compativel(s->tipo, t)) {
            ERRO_NO(an, "SEM003", init,
                    "Não é possível atribuir %s a %s sem "
                    "conversão permitida (destino " ABRE_ASPAS "%s"
                    FECHA_ASPAS "; expressão " ABRE_ASPAS "%s"
                    FECHA_ASPAS ").", ts(t), ts(s->tipo), d->b,
                    texto(an, init));
        }
    }

    /* so declara depois do init, entao int x = x; da erro */
    declarar(an, s);
}

static void condicao(Analisador *an, const AstNode *cond, const char *cmd) {
    Tipo t = valor(an, cond, "condição");
    if (!eh_erro(t) && !tipo_eq(t, T_BOOL)) {
        ERRO_NO(an, "SEM005", cond,
                "Condição de %s deve ter tipo bool; recebeu %s "
                "(expressão " ABRE_ASPAS "%s" FECHA_ASPAS ").",
                cmd, ts(t), texto(an, cond));
    }
}

static void retorno(Analisador *an, const AstNode *r) {
    const AstNode *f = an->funcao_atual;
    const AstNode *v = r->kids[0];

    if (!f) {
        if (v) expr(an, v);
        ERRO_NO(an, "SEM010", r, "Comando return fora de função.");
        return;
    }

    Tipo esperado = tipo_de(f->a, 0);
    if (!v) {
        if (!tipo_eq(esperado, T_VOID)) {
            ERRO_NO(an, "SEM010", r,
                    "A função " ABRE_ASPAS "%s" FECHA_ASPAS
                    " deve retornar %s, mas o return não fornece valor.",
                    f->b, ts(esperado));
        }
        return;
    }

    if (tipo_eq(esperado, T_VOID)) {
        expr(an, v);
        ERRO_NO(an, "SEM010", v,
                "A função " ABRE_ASPAS "%s" FECHA_ASPAS " tem "
                "retorno void e não pode retornar valor (expressão "
                ABRE_ASPAS "%s" FECHA_ASPAS ").", f->b, texto(an, v));
        return;
    }

    Tipo t = valor(an, v, "valor de retorno");
    if (!compativel(esperado, t)) {
        ERRO_NO(an, "SEM009", v,
                "Retorno %s incompatível com o tipo %s da "
                "função " ABRE_ASPAS "%s" FECHA_ASPAS "; "
                "conversão implícita de %s para %s não "
                "permitida.", ts(t), ts(esperado), f->b, ts(t), ts(esperado));
    }
}

static void comando(Analisador *an, const AstNode *cmd) {
    switch (cmd->kind) {
        case N_VARDECL:
            decl_var(an, cmd);
            break;
        case N_BLOCK:
            abrir_escopo(an);
            for (size_t i = 0; i < cmd->nkids; i++) comando(an, cmd->kids[i]);
            fechar_escopo(an);
            break;
        case N_IF:
            condicao(an, cmd->kids[0], "if");
            comando(an, cmd->kids[1]);
            if (cmd->kids[2]) comando(an, cmd->kids[2]);
            break;
        case N_WHILE:
            condicao(an, cmd->kids[0], "while");
            comando(an, cmd->kids[1]);
            break;
        case N_RETURN:
            retorno(an, cmd);
            break;
        case N_EXPRSTMT:
            expr(an, cmd->kids[0]);
            break;
        default:
            break;
    }
}

static int sempre_retorna(const AstNode *cmd) {
    if (!cmd) return 0;
    switch (cmd->kind) {
        case N_RETURN:
            return 1;
        case N_BLOCK:
            for (size_t i = 0; i < cmd->nkids; i++) {
                if (sempre_retorna(cmd->kids[i])) return 1;
            }
            return 0;
        case N_IF:
            return cmd->kids[2] != NULL && sempre_retorna(cmd->kids[1])
                   && sempre_retorna(cmd->kids[2]);
        default:
            return 0;
    }
}

static const char *motivo_queda(Analisador *an, const AstNode *cmd) {
    Sb sb = {0};
    if (cmd->kind == N_BLOCK) {
        if (cmd->nkids == 0) return "o corpo não contém nenhum comando return";
        return motivo_queda(an, cmd->kids[cmd->nkids - 1]);
    }
    if (cmd->kind == N_IF) {
        const char *cond = texto(an, cmd->kids[0]);
        int entao_ok = sempre_retorna(cmd->kids[1]);
        if (!cmd->kids[2]) {
            if (!entao_ok) return motivo_queda(an, cmd->kids[1]);
            sb_printf(&sb, "o ramo em que " ABRE_ASPAS "%s" FECHA_ASPAS
                      " é falso alcança o fim do corpo", cond);
            return guardar(an, sb_take(&sb));
        }
        if (!entao_ok && !sempre_retorna(cmd->kids[2])) {
            sb_printf(&sb, "os ramos em que " ABRE_ASPAS "%s" FECHA_ASPAS
                      " é verdadeiro e falso alcançam o fim do "
                      "corpo", cond);
            return guardar(an, sb_take(&sb));
        }
        if (!entao_ok) return motivo_queda(an, cmd->kids[1]);
        return motivo_queda(an, cmd->kids[2]);
    }
    if (cmd->kind == N_WHILE) {
        sb_printf(&sb, "o laço while com condição "
                  ABRE_ASPAS "%s" FECHA_ASPAS " pode terminar e alcanç"
                  "ar o fim do corpo", texto(an, cmd->kids[0]));
        return guardar(an, sb_take(&sb));
    }
    return "o fim do corpo é alcançado sem um comando return";
}

static void funcao(Analisador *an, const AstNode *f) {
    const AstNode *anterior = an->funcao_atual;
    const AstNode *corpo = f->kids[f->nkids - 1];
    an->funcao_atual = f;

    abrir_escopo(an);
    for (size_t i = 0; i < f->nparams; i++) {
        const AstNode *p = f->kids[i];
        if (strcmp(p->a, "void") == 0) {
            ERRO_NO(an, "SEM014", p,
                    "Parâmetro " ABRE_ASPAS "%s" FECHA_ASPAS " não "
                    "pode ter tipo void.", p->b);
        }
        declarar(an, novo_simbolo(an, p->b, CAT_PARAMETRO,
                                  tipo_de(p->a, p->vetor), p->linha, p->coluna));
    }
    /* params e corpo no mesmo escopo */
    for (size_t i = 0; i < corpo->nkids; i++) comando(an, corpo->kids[i]);
    fechar_escopo(an);

    if (strcmp(f->a, "void") != 0 && !sempre_retorna(corpo)) {
        const char *motivo = motivo_queda(an, corpo);
        ERRO_NO(an, "SEM011", f,
                "A função " ABRE_ASPAS "%s" FECHA_ASPAS " pode "
                "terminar sem retornar %s; %s.", f->b, f->a, motivo);
    }
    an->funcao_atual = anterior;
}

/* junta as assinaturas antes dos corpos, por causa da recursao */
static void coletar_assinaturas(Analisador *an, const AstNode *programa) {
    ListaSimbolos vistos = {0};
    for (size_t i = 0; i < programa->nkids; i++) {
        const AstNode *item = programa->kids[i];
        if (item->kind == N_FUNCTION) {
            Simbolo *s = assinatura(an, item);
            Simbolo *anterior = lista_buscar(&vistos, item->b);
            if (anterior) {
                erro_duplicado(an, s, anterior);
                continue;
            }
            lista_add(&vistos, s);
            inserir(an, s);
        } else if (item->kind == N_VARDECL && !lista_buscar(&vistos, item->b)) {
            lista_add(&vistos, simbolo_var(an, item));
        }
    }
    free(vistos.itens);
}

static int comparar_diags(const void *x, const void *y) {
    const Diagnostico *a = (const Diagnostico *) x;
    const Diagnostico *b = (const Diagnostico *) y;
    if (a->linha != b->linha) return a->linha < b->linha ? -1 : 1;
    if (a->coluna != b->coluna) return a->coluna < b->coluna ? -1 : 1;
    if (a->ordem != b->ordem) return a->ordem < b->ordem ? -1 : 1;
    return 0;
}

int semantic_analisar(const AstNode *programa, char **saida) {
    Analisador an;
    memset(&an, 0, sizeof(an));
    abrir_escopo(&an);

    coletar_assinaturas(&an, programa);
    for (size_t i = 0; i < programa->nkids; i++) {
        const AstNode *item = programa->kids[i];
        if (item->kind == N_FUNCTION) funcao(&an, item);
        else if (item->kind == N_VARDECL) decl_var(&an, item);
        else comando(&an, item);
    }

    if (an.ndiags > 1) {
        qsort(an.diags, an.ndiags, sizeof(Diagnostico), comparar_diags);
    }

    Sb sb = {0};
    for (size_t i = 0; i < an.ndiags; i++) {
        const Diagnostico *d = &an.diags[i];
        sb_printf(&sb, "%s " TRAVESSAO " linha %d, coluna %d: %s" SEPARADOR,
                  d->codigo, d->linha, d->coluna, d->mensagem);
    }
    sb_printf(&sb, "Análise semântica concluída: "
              "%lu %s; programa %s.", (unsigned long) an.ndiags,
              an.ndiags == 1 ? "erro" : "erros",
              an.ndiags == 0 ? "aceito" : "rejeitado");
    *saida = sb_take(&sb);

    int total = (int) an.ndiags;

    while (an.nescopos > 0) fechar_escopo(&an);
    free(an.escopos);
    for (size_t i = 0; i < an.todos.n; i++) {
        free(an.todos.itens[i]->params);
        free(an.todos.itens[i]);
    }
    free(an.todos.itens);
    for (size_t i = 0; i < an.ndiags; i++) free(an.diags[i].mensagem);
    free(an.diags);
    for (size_t i = 0; i < an.ntextos; i++) free(an.textos[i]);
    free(an.textos);

    return total;
}
