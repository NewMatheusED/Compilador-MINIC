#ifndef MINIC_LEXER_H
#define MINIC_LEXER_H

#include <stddef.h>

/* Categorias de token da linguagem MINIC (secao 3 da especificacao). */
typedef enum {
    TOK_ID,

    /* Palavras reservadas */
    TOK_KW_INT, TOK_KW_FLOAT, TOK_KW_BOOL, TOK_KW_CHAR, TOK_KW_VOID,
    TOK_KW_IF, TOK_KW_ELSE, TOK_KW_WHILE, TOK_KW_FOR,
    TOK_KW_RETURN, TOK_KW_BREAK, TOK_KW_CONTINUE,
    TOK_KW_TRUE, TOK_KW_FALSE,
    TOK_KW_PRINT, TOK_KW_READ,

    /* Literais */
    TOK_INT_LIT, TOK_FLOAT_LIT, TOK_CHAR_LIT, TOK_STRING_LIT,

    /* Operadores aritmeticos */
    TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH, TOK_PERCENT,

    /* Operadores relacionais */
    TOK_EQ, TOK_NEQ, TOK_LT, TOK_GT, TOK_LE, TOK_GE,

    /* Operadores logicos */
    TOK_AND, TOK_OR, TOK_NOT,

    /* Atribuicao */
    TOK_ASSIGN,

    /* Delimitadores */
    TOK_LPAREN, TOK_RPAREN, TOK_LBRACKET, TOK_RBRACKET,
    TOK_LBRACE, TOK_RBRACE, TOK_SEMI, TOK_COMMA,

    TOK_EOF
} TokenType;

typedef struct {
    TokenType type;
    char *lexeme;   /* texto reconhecido, alocado dinamicamente */
    int line;
    int col;
} Token;

/* Callback chamado a cada diagnostico lexico. Cada front-end decide o
 * formato (texto da especificacao, JSON Lines, ou nenhum). */
typedef void (*LexerErrorSink)(const char *msg, int line, int col);

typedef struct {
    const char *src;
    size_t pos;
    size_t len;
    int line;
    int col;
    int error_count;
    LexerErrorSink on_error;
} Lexer;

void lexer_init(Lexer *lx, const char *src);

/* Troca o destino dos diagnosticos. Sem chamada, vale o formato textual
 * da especificacao (secao 12), impresso na saida padrao. */
void lexer_set_error_sink(Lexer *lx, LexerErrorSink sink);

/* Formato textual padrao, exposto para quem quiser reaproveitar. */
void lexer_default_error_sink(const char *msg, int line, int col);

/* Retorna o proximo token. Erros lexicos sao reportados em stdout no
 * formato definido pela especificacao (secao 12) e o scanner tenta se
 * recuperar para continuar emitindo tokens. */
Token lexer_next_token(Lexer *lx);

void token_free(Token *tok);
const char *token_type_name(TokenType type);

#endif
