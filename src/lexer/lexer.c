#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "lexer.h"

/* ---------- utilidades internas ---------- */

static char peek(Lexer *lx) {
    if (lx->pos >= lx->len) return '\0';
    return lx->src[lx->pos];
}

static char peek_at(Lexer *lx, size_t offset) {
    if (lx->pos + offset >= lx->len) return '\0';
    return lx->src[lx->pos + offset];
}

static char advance(Lexer *lx) {
    char c = lx->src[lx->pos++];
    if (c == '\n') {
        lx->line++;
        lx->col = 1;
    } else {
        lx->col++;
    }
    return c;
}

static int at_end(Lexer *lx) {
    return lx->pos >= lx->len;
}

static void report_error(Lexer *lx, int line, int col, const char *msg) {
    printf("Erro lexico na linha %d, coluna %d:\n%s.\n", line, col, msg);
    lx->error_count++;
}

static char *make_lexeme(const char *start, size_t len) {
    char *s = (char *) malloc(len + 1);
    memcpy(s, start, len);
    s[len] = '\0';
    return s;
}

static Token make_token(TokenType type, const char *start, size_t len, int line, int col) {
    Token t;
    t.type = type;
    t.lexeme = make_lexeme(start, len);
    t.line = line;
    t.col = col;
    return t;
}

/* ---------- tabela de palavras reservadas ---------- */

typedef struct {
    const char *word;
    TokenType type;
} Keyword;

static const Keyword KEYWORDS[] = {
    {"int", TOK_KW_INT}, {"float", TOK_KW_FLOAT}, {"bool", TOK_KW_BOOL},
    {"char", TOK_KW_CHAR}, {"void", TOK_KW_VOID},
    {"if", TOK_KW_IF}, {"else", TOK_KW_ELSE},
    {"while", TOK_KW_WHILE}, {"for", TOK_KW_FOR},
    {"return", TOK_KW_RETURN}, {"break", TOK_KW_BREAK}, {"continue", TOK_KW_CONTINUE},
    {"true", TOK_KW_TRUE}, {"false", TOK_KW_FALSE},
    {"print", TOK_KW_PRINT}, {"read", TOK_KW_READ},
    {NULL, TOK_EOF}
};

static TokenType lookup_keyword(const char *text) {
    for (int i = 0; KEYWORDS[i].word != NULL; i++) {
        if (strcmp(KEYWORDS[i].word, text) == 0) {
            return KEYWORDS[i].type;
        }
    }
    return TOK_ID;
}

/* ---------- espacos e comentarios ---------- */

static void skip_whitespace_and_comments(Lexer *lx) {
    for (;;) {
        char c = peek(lx);
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance(lx);
        } else if (c == '/' && peek_at(lx, 1) == '/') {
            while (!at_end(lx) && peek(lx) != '\n') advance(lx);
        } else if (c == '/' && peek_at(lx, 1) == '*') {
            int start_line = lx->line, start_col = lx->col;
            advance(lx); advance(lx); /* consome o abre-comentario */
            int closed = 0;
            while (!at_end(lx)) {
                if (peek(lx) == '*' && peek_at(lx, 1) == '/') {
                    advance(lx); advance(lx);
                    closed = 1;
                    break;
                }
                advance(lx);
            }
            if (!closed) {
                report_error(lx, start_line, start_col, "comentario de bloco nao terminado");
            }
        } else {
            break;
        }
    }
}

/* ---------- escapes ---------- */

/* Traduz uma sequencia de escape (o caractere apos a barra invertida).
 * Retorna 1 se valida, 0 se desconhecida. */
static int is_valid_escape(char c) {
    return c == 'n' || c == 't' || c == '\\' || c == '\'' || c == '"';
}

/* ---------- reconhecedores ---------- */

static Token scan_identifier(Lexer *lx) {
    int line = lx->line, col = lx->col;
    size_t start = lx->pos;
    while (isalnum((unsigned char) peek(lx)) || peek(lx) == '_') advance(lx);
    size_t len = lx->pos - start;
    char *text = make_lexeme(lx->src + start, len);
    TokenType type = lookup_keyword(text);
    Token t;
    t.type = type;
    t.lexeme = text;
    t.line = line;
    t.col = col;
    return t;
}

static Token scan_number(Lexer *lx) {
    int line = lx->line, col = lx->col;
    size_t start = lx->pos;
    TokenType type = TOK_INT_LIT;
    while (isdigit((unsigned char) peek(lx))) advance(lx);
    if (peek(lx) == '.' && isdigit((unsigned char) peek_at(lx, 1))) {
        type = TOK_FLOAT_LIT;
        advance(lx); /* '.' */
        while (isdigit((unsigned char) peek(lx))) advance(lx);
    }
    size_t len = lx->pos - start;
    return make_token(type, lx->src + start, len, line, col);
}

static Token scan_char_literal(Lexer *lx) {
    int line = lx->line, col = lx->col;
    size_t start = lx->pos;
    advance(lx); /* abre aspas simples */

    if (peek(lx) == '\'') {
        /* '' vazio */
        advance(lx);
        size_t len = lx->pos - start;
        report_error(lx, line, col, "literal de caractere com tamanho invalido");
        return make_token(TOK_CHAR_LIT, lx->src + start, len, line, col);
    }

    if (at_end(lx) || peek(lx) == '\n') {
        report_error(lx, line, col, "literal de caractere nao terminado");
        size_t len = lx->pos - start;
        return make_token(TOK_CHAR_LIT, lx->src + start, len, line, col);
    }

    if (peek(lx) == '\\') {
        advance(lx);
        char esc = peek(lx);
        if (at_end(lx) || esc == '\n') {
            report_error(lx, line, col, "literal de caractere nao terminado");
            size_t len = lx->pos - start;
            return make_token(TOK_CHAR_LIT, lx->src + start, len, line, col);
        }
        if (!is_valid_escape(esc)) {
            char msg[64];
            snprintf(msg, sizeof(msg), "sequencia de escape desconhecida \"\\%c\"", esc);
            report_error(lx, lx->line, lx->col, msg);
        }
        advance(lx);
    } else {
        advance(lx); /* caractere comum */
    }

    if (peek(lx) != '\'') {
        /* mais de um caractere dentro das aspas: consumir ate a proxima
         * aspas simples (na mesma linha) para recuperacao. */
        int bad_line = line, bad_col = col;
        while (!at_end(lx) && peek(lx) != '\'' && peek(lx) != '\n') advance(lx);
        if (peek(lx) == '\'') {
            advance(lx);
            size_t len = lx->pos - start;
            report_error(lx, bad_line, bad_col, "literal de caractere com tamanho invalido");
            return make_token(TOK_CHAR_LIT, lx->src + start, len, line, col);
        } else {
            size_t len = lx->pos - start;
            report_error(lx, bad_line, bad_col, "literal de caractere nao terminado");
            return make_token(TOK_CHAR_LIT, lx->src + start, len, line, col);
        }
    }

    advance(lx); /* fecha aspas simples */
    size_t len = lx->pos - start;
    return make_token(TOK_CHAR_LIT, lx->src + start, len, line, col);
}

static Token scan_string_literal(Lexer *lx) {
    int line = lx->line, col = lx->col;
    size_t start = lx->pos;
    advance(lx); /* abre aspas duplas */

    while (!at_end(lx) && peek(lx) != '"' && peek(lx) != '\n') {
        if (peek(lx) == '\\') {
            advance(lx);
            if (at_end(lx) || peek(lx) == '\n') break;
            char esc = peek(lx);
            if (!is_valid_escape(esc)) {
                char msg[64];
                snprintf(msg, sizeof(msg), "sequencia de escape desconhecida \"\\%c\"", esc);
                report_error(lx, lx->line, lx->col, msg);
            }
            advance(lx);
        } else {
            advance(lx);
        }
    }

    if (at_end(lx) || peek(lx) != '"') {
        report_error(lx, line, col, "literal de cadeia nao terminado");
        size_t len = lx->pos - start;
        return make_token(TOK_STRING_LIT, lx->src + start, len, line, col);
    }

    advance(lx); /* fecha aspas duplas */
    size_t len = lx->pos - start;
    return make_token(TOK_STRING_LIT, lx->src + start, len, line, col);
}

/* ---------- API publica ---------- */

void lexer_init(Lexer *lx, const char *src) {
    lx->src = src;
    lx->pos = 0;
    lx->len = strlen(src);
    lx->line = 1;
    lx->col = 1;
    lx->error_count = 0;
}

Token lexer_next_token(Lexer *lx) {
    for (;;) {
        skip_whitespace_and_comments(lx);

        if (at_end(lx)) {
            return make_token(TOK_EOF, "", 0, lx->line, lx->col);
        }

        char c = peek(lx);
        int line = lx->line, col = lx->col;

        if (isalpha((unsigned char) c) || c == '_') {
            return scan_identifier(lx);
        }
        if (isdigit((unsigned char) c)) {
            return scan_number(lx);
        }
        if (c == '\'') {
            return scan_char_literal(lx);
        }
        if (c == '"') {
            return scan_string_literal(lx);
        }

        /* operadores de dois caracteres */
        if (c == '=' && peek_at(lx, 1) == '=') { advance(lx); advance(lx); return make_token(TOK_EQ, "==", 2, line, col); }
        if (c == '!' && peek_at(lx, 1) == '=') { advance(lx); advance(lx); return make_token(TOK_NEQ, "!=", 2, line, col); }
        if (c == '<' && peek_at(lx, 1) == '=') { advance(lx); advance(lx); return make_token(TOK_LE, "<=", 2, line, col); }
        if (c == '>' && peek_at(lx, 1) == '=') { advance(lx); advance(lx); return make_token(TOK_GE, ">=", 2, line, col); }
        if (c == '&' && peek_at(lx, 1) == '&') { advance(lx); advance(lx); return make_token(TOK_AND, "&&", 2, line, col); }
        if (c == '|' && peek_at(lx, 1) == '|') { advance(lx); advance(lx); return make_token(TOK_OR, "||", 2, line, col); }

        /* operadores e delimitadores de um caractere */
        switch (c) {
            case '+': advance(lx); return make_token(TOK_PLUS, "+", 1, line, col);
            case '-': advance(lx); return make_token(TOK_MINUS, "-", 1, line, col);
            case '*': advance(lx); return make_token(TOK_STAR, "*", 1, line, col);
            case '/': advance(lx); return make_token(TOK_SLASH, "/", 1, line, col);
            case '%': advance(lx); return make_token(TOK_PERCENT, "%", 1, line, col);
            case '<': advance(lx); return make_token(TOK_LT, "<", 1, line, col);
            case '>': advance(lx); return make_token(TOK_GT, ">", 1, line, col);
            case '!': advance(lx); return make_token(TOK_NOT, "!", 1, line, col);
            case '=': advance(lx); return make_token(TOK_ASSIGN, "=", 1, line, col);
            case '(': advance(lx); return make_token(TOK_LPAREN, "(", 1, line, col);
            case ')': advance(lx); return make_token(TOK_RPAREN, ")", 1, line, col);
            case '[': advance(lx); return make_token(TOK_LBRACKET, "[", 1, line, col);
            case ']': advance(lx); return make_token(TOK_RBRACKET, "]", 1, line, col);
            case '{': advance(lx); return make_token(TOK_LBRACE, "{", 1, line, col);
            case '}': advance(lx); return make_token(TOK_RBRACE, "}", 1, line, col);
            case ';': advance(lx); return make_token(TOK_SEMI, ";", 1, line, col);
            case ',': advance(lx); return make_token(TOK_COMMA, ",", 1, line, col);
            default: break;
        }

        /* simbolo nao reconhecido: reporta e descarta o caractere */
        char msg[64];
        snprintf(msg, sizeof(msg), "simbolo \"%c\" nao reconhecido", c);
        report_error(lx, line, col, msg);
        advance(lx);
        /* continua o laco para tentar reconhecer o proximo token */
    }
}

void token_free(Token *tok) {
    free(tok->lexeme);
    tok->lexeme = NULL;
}

const char *token_type_name(TokenType type) {
    switch (type) {
        case TOK_ID: return "ID";
        case TOK_KW_INT: return "KW_INT";
        case TOK_KW_FLOAT: return "KW_FLOAT";
        case TOK_KW_BOOL: return "KW_BOOL";
        case TOK_KW_CHAR: return "KW_CHAR";
        case TOK_KW_VOID: return "KW_VOID";
        case TOK_KW_IF: return "KW_IF";
        case TOK_KW_ELSE: return "KW_ELSE";
        case TOK_KW_WHILE: return "KW_WHILE";
        case TOK_KW_FOR: return "KW_FOR";
        case TOK_KW_RETURN: return "KW_RETURN";
        case TOK_KW_BREAK: return "KW_BREAK";
        case TOK_KW_CONTINUE: return "KW_CONTINUE";
        case TOK_KW_TRUE: return "KW_TRUE";
        case TOK_KW_FALSE: return "KW_FALSE";
        case TOK_KW_PRINT: return "KW_PRINT";
        case TOK_KW_READ: return "KW_READ";
        case TOK_INT_LIT: return "INT_LIT";
        case TOK_FLOAT_LIT: return "FLOAT_LIT";
        case TOK_CHAR_LIT: return "CHAR_LIT";
        case TOK_STRING_LIT: return "STRING_LIT";
        case TOK_PLUS: return "PLUS";
        case TOK_MINUS: return "MINUS";
        case TOK_STAR: return "STAR";
        case TOK_SLASH: return "SLASH";
        case TOK_PERCENT: return "PERCENT";
        case TOK_EQ: return "EQ";
        case TOK_NEQ: return "NEQ";
        case TOK_LT: return "LT";
        case TOK_GT: return "GT";
        case TOK_LE: return "LE";
        case TOK_GE: return "GE";
        case TOK_AND: return "AND";
        case TOK_OR: return "OR";
        case TOK_NOT: return "NOT";
        case TOK_ASSIGN: return "ASSIGN";
        case TOK_LPAREN: return "LPAREN";
        case TOK_RPAREN: return "RPAREN";
        case TOK_LBRACKET: return "LBRACKET";
        case TOK_RBRACKET: return "RBRACKET";
        case TOK_LBRACE: return "LBRACE";
        case TOK_RBRACE: return "RBRACE";
        case TOK_SEMI: return "SEMI";
        case TOK_COMMA: return "COMMA";
        case TOK_EOF: return "EOF";
    }
    return "DESCONHECIDO";
}
