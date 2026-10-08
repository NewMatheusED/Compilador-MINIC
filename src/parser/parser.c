#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "../lexer/lexer.h"

/* Mesma gramatica do lado Python (src/parser/minic_parser.py).
 * Os erros usam longjmp para desempilhar a recursao sem propagar
 * codigo de retorno em toda funcao. */

#define LOOKAHEAD 4

typedef struct {
    Lexer lx;
    Token buf[LOOKAHEAD];
    int nbuf;
    jmp_buf escape;
    ErroSintatico *erro;
} Parser;

/* ------------------------------------------------------------------ */
/* Nomes de token usados nos diagnosticos                             */
/* ------------------------------------------------------------------ */

static const char *nome_token(TokenType t) {
    switch (t) {
        case TOK_ID:         return "IDENT";
        case TOK_INT_LIT:    return "LITERAL_INT";
        case TOK_FLOAT_LIT:  return "LITERAL_REAL";
        case TOK_CHAR_LIT:   return "LITERAL_CHAR";
        case TOK_STRING_LIT: return "LITERAL_STRING";
        case TOK_LPAREN:     return "ABRE_PAREN";
        case TOK_RPAREN:     return "FECHA_PAREN";
        case TOK_LBRACE:     return "ABRE_CHAVE";
        case TOK_RBRACE:     return "FECHA_CHAVE";
        case TOK_LBRACKET:   return "ABRE_COLCHETE";
        case TOK_RBRACKET:   return "FECHA_COLCHETE";
        case TOK_SEMI:       return "PONTO_E_VIRGULA";
        case TOK_COMMA:      return "VIRGULA";
        case TOK_ASSIGN:     return "ATRIBUICAO";
        case TOK_EOF:        return "FIM_DE_ARQUIVO";
        default:             return token_type_name(t);
    }
}

static const char *nome_tipo(TokenType t) {
    switch (t) {
        case TOK_KW_INT:   return "int";
        case TOK_KW_FLOAT: return "float";
        case TOK_KW_BOOL:  return "bool";
        case TOK_KW_CHAR:  return "char";
        case TOK_KW_VOID:  return "void";
        default:           return NULL;
    }
}

static int eh_tipo(TokenType t) { return nome_tipo(t) != NULL; }

#define TIPOS_ACEITOS "KW_INT, KW_FLOAT, KW_BOOL, KW_CHAR ou KW_VOID"

/* ------------------------------------------------------------------ */
/* Acesso aos tokens                                                  */
/* ------------------------------------------------------------------ */

static void encher(Parser *p, int quantos) {
    while (p->nbuf < quantos) {
        p->buf[p->nbuf++] = lexer_next_token(&p->lx);
    }
}

static Token *espiar(Parser *p, int adiante) {
    encher(p, adiante + 1);
    return &p->buf[adiante];
}

static Token avancar(Parser *p) {
    encher(p, 1);
    Token tok = p->buf[0];
    for (int i = 1; i < p->nbuf; i++) p->buf[i - 1] = p->buf[i];
    p->nbuf--;
    return tok;
}

static int conferir(Parser *p, TokenType t) {
    return espiar(p, 0)->type == t;
}

static int aceitar(Parser *p, TokenType t) {
    if (!conferir(p, t)) return 0;
    Token tok = avancar(p);
    token_free(&tok);
    return 1;
}

static void erro(Parser *p, const char *msg) {
    Token *tok = espiar(p, 0);
    p->erro->linha = tok->line;
    p->erro->coluna = tok->col;
    if (tok->type == TOK_EOF) {
        snprintf(p->erro->mensagem, sizeof(p->erro->mensagem),
                 "%s, encontrado FIM_DE_ARQUIVO", msg);
    } else {
        snprintf(p->erro->mensagem, sizeof(p->erro->mensagem),
                 "%s, encontrado %s (\"%s\")", msg, nome_token(tok->type),
                 tok->lexeme ? tok->lexeme : "");
    }
    longjmp(p->escape, 1);
}

/* Consome o token esperado e devolve o lexema (o chamador libera). */
static char *exigir_lexema(Parser *p, TokenType t, const char *esperado) {
    if (!conferir(p, t)) {
        char msg[128];
        snprintf(msg, sizeof(msg), "esperado %s", esperado);
        erro(p, msg);
    }
    Token tok = avancar(p);
    return tok.lexeme; /* transferido para o chamador */
}

static Token exigir_token(Parser *p, TokenType t, const char *esperado) {
    if (!conferir(p, t)) {
        char msg[128];
        snprintf(msg, sizeof(msg), "esperado %s", esperado);
        erro(p, msg);
    }
    return avancar(p);
}

static void exigir(Parser *p, TokenType t, const char *esperado) {
    char *lex = exigir_lexema(p, t, esperado);
    free(lex);
}

/* ------------------------------------------------------------------ */
/* Declaracoes antecipadas                                            */
/* ------------------------------------------------------------------ */

static AstNode *p_item(Parser *p);
static AstNode *p_funcao(Parser *p);
static AstNode *p_decl_var(Parser *p);
static AstNode *p_comando(Parser *p);
static AstNode *p_bloco(Parser *p);
static AstNode *p_expr(Parser *p);
static AstNode *p_atribuicao(Parser *p);
static AstNode *p_unaria(Parser *p);
static AstNode *p_posfixa(Parser *p);
static AstNode *p_primaria(Parser *p);

/* ------------------------------------------------------------------ */
/* Nivel global                                                       */
/* ------------------------------------------------------------------ */

static AstNode *p_item(Parser *p) {
    Token *tok = espiar(p, 0);

    if (eh_tipo(tok->type)) {
        if (espiar(p, 1)->type == TOK_ID && espiar(p, 2)->type == TOK_LPAREN) {
            return p_funcao(p);
        }
        return p_decl_var(p);
    }

    if (tok->type == TOK_RBRACE) {
        erro(p, "token FECHA_CHAVE inesperado no nivel global");
    }
    if (tok->type == TOK_KW_ELSE) {
        erro(p, "token KW_ELSE inesperado (nao ha if correspondente)");
    }

    return p_comando(p);
}

static AstNode *p_param(Parser *p, int depois_de_virgula) {
    Token *tok = espiar(p, 0);
    if (!eh_tipo(tok->type)) {
        if (depois_de_virgula) {
            erro(p, "esperado tipo (" TIPOS_ACEITOS ") ou FECHA_PAREN");
        }
        erro(p, "esperado tipo (" TIPOS_ACEITOS ")");
    }
    Token t = avancar(p);
    const char *tipo = nome_tipo(t.type);
    Token nome = exigir_token(p, TOK_ID, "IDENT");
    AstNode *no = ast_em(ast_new(N_PARAM, tipo, nome.lexeme),
                         nome.line, nome.col);
    token_free(&nome);
    token_free(&t);
    if (aceitar(p, TOK_LBRACKET)) {
        exigir(p, TOK_RBRACKET, "FECHA_COLCHETE");
        no->vetor = 1;
    }
    return no;
}

static AstNode *p_funcao(Parser *p) {
    Token t = avancar(p);
    const char *tipo = nome_tipo(t.type);
    Token nome = exigir_token(p, TOK_ID, "IDENT");

    AstNode *no = ast_em(ast_new(N_FUNCTION, tipo, nome.lexeme),
                         t.line, t.col);
    no->linha2 = nome.line;
    no->coluna2 = nome.col;
    token_free(&nome);
    token_free(&t);

    exigir(p, TOK_LPAREN, "ABRE_PAREN");

    if (!conferir(p, TOK_RPAREN)) {
        ast_add(no, p_param(p, 0));
        no->nparams++;
        while (aceitar(p, TOK_COMMA)) {
            ast_add(no, p_param(p, 1));
            no->nparams++;
        }
    }

    if (!conferir(p, TOK_RPAREN)) erro(p, "esperado VIRGULA ou FECHA_PAREN");
    exigir(p, TOK_RPAREN, "FECHA_PAREN");

    if (!conferir(p, TOK_LBRACE)) {
        /* `int f();` -- prototipo nao faz parte do subconjunto */
        erro(p, "esperado ABRE_CHAVE (funcao precisa de corpo)");
    }
    ast_add(no, p_bloco(p));
    return no;
}

static AstNode *p_decl_var(Parser *p) {
    Token t = avancar(p);
    const char *tipo = nome_tipo(t.type);
    Token nome = exigir_token(p, TOK_ID, "IDENT");

    AstNode *no = ast_em(ast_new(N_VARDECL, tipo, nome.lexeme),
                         nome.line, nome.col);
    token_free(&nome);
    token_free(&t);

    ast_add(no, NULL); /* kids[0]: tamanho  */
    ast_add(no, NULL); /* kids[1]: init     */

    if (aceitar(p, TOK_LBRACKET)) {
        if (conferir(p, TOK_RBRACKET)) erro(p, "esperado expressao no tamanho do vetor");
        no->kids[0] = p_expr(p);
        exigir(p, TOK_RBRACKET, "FECHA_COLCHETE");
    }

    if (aceitar(p, TOK_ASSIGN)) {
        if (conferir(p, TOK_SEMI)) erro(p, "esperado expressao no inicializador");
        no->kids[1] = p_expr(p);
    }

    exigir(p, TOK_SEMI, "PONTO_E_VIRGULA");
    return no;
}

/* ------------------------------------------------------------------ */
/* Comandos                                                           */
/* ------------------------------------------------------------------ */

static AstNode *p_comando_if(Parser *p) {
    Token kw = avancar(p);
    AstNode *no = ast_em(ast_new(N_IF, NULL, NULL), kw.line, kw.col);
    token_free(&kw);

    exigir(p, TOK_LPAREN, "ABRE_PAREN");
    if (conferir(p, TOK_RPAREN)) erro(p, "esperado expressao na condicao");
    ast_add(no, p_expr(p));
    exigir(p, TOK_RPAREN, "FECHA_PAREN");
    ast_add(no, p_comando(p));
    ast_add(no, aceitar(p, TOK_KW_ELSE) ? p_comando(p) : NULL);
    return no;
}

static AstNode *p_comando_while(Parser *p) {
    Token kw = avancar(p);
    AstNode *no = ast_em(ast_new(N_WHILE, NULL, NULL), kw.line, kw.col);
    token_free(&kw);

    exigir(p, TOK_LPAREN, "ABRE_PAREN");
    if (conferir(p, TOK_RPAREN)) erro(p, "esperado expressao na condicao");
    ast_add(no, p_expr(p));
    exigir(p, TOK_RPAREN, "FECHA_PAREN");
    ast_add(no, p_comando(p));
    return no;
}

static AstNode *p_comando_return(Parser *p) {
    Token kw = avancar(p);
    AstNode *no = ast_em(ast_new(N_RETURN, NULL, NULL), kw.line, kw.col);
    token_free(&kw);

    if (aceitar(p, TOK_SEMI)) {
        ast_add(no, NULL);
        return no;
    }
    ast_add(no, p_expr(p));
    exigir(p, TOK_SEMI, "PONTO_E_VIRGULA");
    return no;
}

static AstNode *p_bloco(Parser *p) {
    Token abre = exigir_token(p, TOK_LBRACE, "ABRE_CHAVE");
    AstNode *no = ast_em(ast_new(N_BLOCK, NULL, NULL), abre.line, abre.col);
    token_free(&abre);
    while (!conferir(p, TOK_RBRACE)) {
        if (conferir(p, TOK_EOF)) erro(p, "esperado FECHA_CHAVE");
        ast_add(no, p_comando(p));
    }
    exigir(p, TOK_RBRACE, "FECHA_CHAVE");
    return no;
}

static AstNode *p_comando(Parser *p) {
    Token *tok = espiar(p, 0);

    if (eh_tipo(tok->type))      return p_decl_var(p);
    if (tok->type == TOK_LBRACE) return p_bloco(p);
    if (tok->type == TOK_KW_IF)     return p_comando_if(p);
    if (tok->type == TOK_KW_WHILE)  return p_comando_while(p);
    if (tok->type == TOK_KW_RETURN) return p_comando_return(p);

    if (tok->type == TOK_KW_ELSE) {
        erro(p, "token KW_ELSE inesperado (nao ha if correspondente)");
    }
    if (tok->type == TOK_RBRACE || tok->type == TOK_EOF) {
        erro(p, "esperado inicio de statement");
    }

    AstNode *expr = p_expr(p);
    AstNode *no = ast_em(ast_new(N_EXPRSTMT, NULL, NULL),
                         expr->linha, expr->coluna);
    ast_add(no, expr);
    exigir(p, TOK_SEMI, "PONTO_E_VIRGULA");
    return no;
}

/* ------------------------------------------------------------------ */
/* Expressoes                                                         */
/* ------------------------------------------------------------------ */

static AstNode *p_expr(Parser *p) { return p_atribuicao(p); }

static AstNode *bin(const char *op, const Token *tok_op,
                    AstNode *esq, AstNode *dir) {
    AstNode *no = ast_em(ast_new(N_BINARY, op, NULL), esq->linha, esq->coluna);
    no->linha2 = tok_op->line;
    no->coluna2 = tok_op->col;
    ast_add(no, esq);
    ast_add(no, dir);
    return no;
}

static const char *op_ou(TokenType t)  { return t == TOK_OR ? "||" : NULL; }
static const char *op_e(TokenType t)   { return t == TOK_AND ? "&&" : NULL; }

static const char *op_igualdade(TokenType t) {
    if (t == TOK_EQ) return "==";
    if (t == TOK_NEQ) return "!=";
    return NULL;
}

static const char *op_relacional(TokenType t) {
    if (t == TOK_LT) return "<";
    if (t == TOK_LE) return "<=";
    if (t == TOK_GT) return ">";
    if (t == TOK_GE) return ">=";
    return NULL;
}

static const char *op_aditivo(TokenType t) {
    if (t == TOK_PLUS) return "+";
    if (t == TOK_MINUS) return "-";
    return NULL;
}

static const char *op_multiplicativo(TokenType t) {
    if (t == TOK_STAR) return "*";
    if (t == TOK_SLASH) return "/";
    if (t == TOK_PERCENT) return "%";
    return NULL;
}

static const char *op_unario(TokenType t) {
    if (t == TOK_MINUS) return "-";
    if (t == TOK_NOT) return "!";
    if (t == TOK_PLUS) return "+";
    return NULL;
}

/* Cada nivel de precedencia e um laco identico; a diferenca e a tabela
 * de operadores e o proximo nivel. */
#define NIVEL(nome, proximo, tabela)                                   \
    static AstNode *nome(Parser *p) {                                  \
        AstNode *no = proximo(p);                                      \
        const char *op;                                                \
        while ((op = tabela(espiar(p, 0)->type)) != NULL) {            \
            Token t = avancar(p);                                      \
            no = bin(op, &t, no, proximo(p));                          \
            token_free(&t);                                            \
        }                                                              \
        return no;                                                     \
    }

NIVEL(p_multiplicativa, p_unaria,         op_multiplicativo)
NIVEL(p_aditiva,        p_multiplicativa, op_aditivo)
NIVEL(p_relacional,     p_aditiva,        op_relacional)
NIVEL(p_igualdade,      p_relacional,     op_igualdade)
NIVEL(p_logico_e,       p_igualdade,      op_e)
NIVEL(p_logico_ou,      p_logico_e,       op_ou)

static AstNode *p_atribuicao(Parser *p) {
    AstNode *esq = p_logico_ou(p);
    /* qualquer lado esquerdo; o semantico barra `3 = n;` */
    if (aceitar(p, TOK_ASSIGN)) {
        AstNode *no = ast_em(ast_new(N_ASSIGN, NULL, NULL),
                             esq->linha, esq->coluna);
        ast_add(no, esq);
        ast_add(no, p_atribuicao(p));
        return no;
    }
    return esq;
}

static AstNode *p_unaria(Parser *p) {
    const char *op = op_unario(espiar(p, 0)->type);
    if (op) {
        Token t = avancar(p);
        AstNode *no = ast_em(ast_new(N_UNARY, op, NULL), t.line, t.col);
        token_free(&t);
        ast_add(no, p_unaria(p));
        return no;
    }
    return p_posfixa(p);
}

static AstNode *p_posfixa(Parser *p) {
    AstNode *no = p_primaria(p);

    for (;;) {
        if (aceitar(p, TOK_LPAREN)) {
            AstNode *chamada = ast_em(ast_new(N_CALL, NULL, NULL),
                                      no->linha, no->coluna);
            ast_add(chamada, no);
            if (!conferir(p, TOK_RPAREN)) {
                ast_add(chamada, p_expr(p));
                while (aceitar(p, TOK_COMMA)) {
                    if (conferir(p, TOK_RPAREN)) {
                        erro(p, "esperado expressao ou FECHA_PAREN");
                    }
                    ast_add(chamada, p_expr(p));
                }
            }
            exigir(p, TOK_RPAREN, "FECHA_PAREN");
            no = chamada;
        } else if (aceitar(p, TOK_LBRACKET)) {
            if (conferir(p, TOK_RBRACKET)) erro(p, "esperado expressao no indice");
            AstNode *idx = ast_em(ast_new(N_INDEX, NULL, NULL),
                                  no->linha, no->coluna);
            ast_add(idx, no);
            ast_add(idx, p_expr(p));
            exigir(p, TOK_RBRACKET, "FECHA_COLCHETE");
            no = idx;
        } else {
            return no;
        }
    }
}

static AstNode *lit(Parser *p, const char *tipo) {
    Token t = avancar(p);
    AstNode *no = ast_em(ast_new(N_LIT, tipo, t.lexeme), t.line, t.col);
    token_free(&t);
    return no;
}

static AstNode *p_primaria(Parser *p) {
    Token *tok = espiar(p, 0);

    switch (tok->type) {
        case TOK_ID: {
            Token t = avancar(p);
            AstNode *no = ast_em(ast_new(N_ID, t.lexeme, NULL), t.line, t.col);
            token_free(&t);
            return no;
        }
        case TOK_INT_LIT:    return lit(p, "int");
        case TOK_FLOAT_LIT:  return lit(p, "real");
        case TOK_CHAR_LIT:   return lit(p, "char");
        case TOK_STRING_LIT: return lit(p, "string");
        case TOK_KW_TRUE:
        case TOK_KW_FALSE:   return lit(p, "bool");
        case TOK_LPAREN: {
            Token t = avancar(p);
            token_free(&t);
            if (conferir(p, TOK_RPAREN)) erro(p, "esperado expressao");
            AstNode *no = p_expr(p);
            exigir(p, TOK_RPAREN, "FECHA_PAREN");
            return no;
        }
        default:
            erro(p, "esperado identificador, literal ou ABRE_PAREN");
            return NULL; /* nao alcancavel */
    }
}

/* ------------------------------------------------------------------ */
/* Ponto de entrada                                                   */
/* ------------------------------------------------------------------ */

/* Durante a analise sintatica os diagnosticos lexicos nao vao para a
 * saida padrao (ela e reservada para a AST). */
static int houve_erro_lexico = 0;

static void silenciar_lexico(const char *msg, int line, int col) {
    (void) msg; (void) line; (void) col;
    houve_erro_lexico = 1;
}

AstNode *parse_source(const char *src, ErroSintatico *erro_saida) {
    Parser p;
    memset(&p, 0, sizeof(p));
    p.erro = erro_saida;
    p.nbuf = 0;
    lexer_init(&p.lx, src);
    lexer_set_error_sink(&p.lx, silenciar_lexico);
    houve_erro_lexico = 0;

    AstNode *programa = ast_new(N_PROGRAM, NULL, NULL);

    if (setjmp(p.escape) != 0) {
        ast_free(programa);
        for (int i = 0; i < p.nbuf; i++) token_free(&p.buf[i]);
        return NULL;
    }

    while (!conferir(&p, TOK_EOF)) {
        ast_add(programa, p_item(&p));
    }

    for (int i = 0; i < p.nbuf; i++) token_free(&p.buf[i]);

    if (houve_erro_lexico || p.lx.error_count > 0) {
        snprintf(erro_saida->mensagem, sizeof(erro_saida->mensagem),
                 "erro lexico na entrada");
        erro_saida->linha = 0;
        erro_saida->coluna = 0;
        ast_free(programa);
        return NULL;
    }

    return programa;
}
