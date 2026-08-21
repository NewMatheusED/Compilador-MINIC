from __future__ import annotations

from dataclasses import dataclass
from enum import Enum, auto
from typing import List, Optional


class TokenType(Enum):
    ID = auto()

    # Palavras reservadas
    KW_INT = auto()
    KW_FLOAT = auto()
    KW_BOOL = auto()
    KW_CHAR = auto()
    KW_VOID = auto()
    KW_IF = auto()
    KW_ELSE = auto()
    KW_WHILE = auto()
    KW_FOR = auto()
    KW_RETURN = auto()
    KW_BREAK = auto()
    KW_CONTINUE = auto()
    KW_TRUE = auto()
    KW_FALSE = auto()
    KW_PRINT = auto()
    KW_READ = auto()

    # Literais
    INT_LIT = auto()
    FLOAT_LIT = auto()
    CHAR_LIT = auto()
    STRING_LIT = auto()

    # Operadores aritmeticos
    PLUS = auto()
    MINUS = auto()
    STAR = auto()
    SLASH = auto()
    PERCENT = auto()

    # Operadores relacionais
    EQ = auto()
    NEQ = auto()
    LT = auto()
    GT = auto()
    LE = auto()
    GE = auto()

    # Operadores logicos
    AND = auto()
    OR = auto()
    NOT = auto()

    # Atribuicao
    ASSIGN = auto()

    # Delimitadores
    LPAREN = auto()
    RPAREN = auto()
    LBRACKET = auto()
    RBRACKET = auto()
    LBRACE = auto()
    RBRACE = auto()
    SEMI = auto()
    COMMA = auto()

    EOF = auto()


KEYWORDS = {
    "int": TokenType.KW_INT,
    "float": TokenType.KW_FLOAT,
    "bool": TokenType.KW_BOOL,
    "char": TokenType.KW_CHAR,
    "void": TokenType.KW_VOID,
    "if": TokenType.KW_IF,
    "else": TokenType.KW_ELSE,
    "while": TokenType.KW_WHILE,
    "for": TokenType.KW_FOR,
    "return": TokenType.KW_RETURN,
    "break": TokenType.KW_BREAK,
    "continue": TokenType.KW_CONTINUE,
    "true": TokenType.KW_TRUE,
    "false": TokenType.KW_FALSE,
    "print": TokenType.KW_PRINT,
    "read": TokenType.KW_READ,
}

VALID_ESCAPES = {"n", "t", "\\", "'", '"'}


@dataclass
class Token:
    type: TokenType
    lexeme: str
    line: int
    col: int


class LexicalError(Exception):
    """Usado apenas internamente para sinalizar diagnosticos formatados."""


class Lexer:
    def __init__(self, source: str, on_error=None):
        self.src = source
        self.pos = 0
        self.len = len(source)
        self.line = 1
        self.col = 1
        self.error_count = 0
        # callback(mensagem, linha, coluna) chamado a cada diagnostico
        self._on_error = on_error

    def _peek(self, offset: int = 0) -> str:
        idx = self.pos + offset
        if idx >= self.len:
            return ""
        return self.src[idx]

    def _advance(self) -> str:
        c = self.src[self.pos]
        self.pos += 1
        if c == "\n":
            self.line += 1
            self.col = 1
        else:
            self.col += 1
        return c

    def _at_end(self) -> bool:
        return self.pos >= self.len

    def _report(self, line: int, col: int, msg: str) -> None:
        self.error_count += 1
        if self._on_error:
            self._on_error(msg, line, col)

    def _skip_whitespace_and_comments(self) -> None:
        while True:
            c = self._peek()
            if c in (" ", "\t", "\r", "\n"):
                self._advance()
            elif c == "/" and self._peek(1) == "/":
                while not self._at_end() and self._peek() != "\n":
                    self._advance()
            elif c == "/" and self._peek(1) == "*":
                start_line, start_col = self.line, self.col
                self._advance()
                self._advance()
                closed = False
                while not self._at_end():
                    if self._peek() == "*" and self._peek(1) == "/":
                        self._advance()
                        self._advance()
                        closed = True
                        break
                    self._advance()
                if not closed:
                    self._report(start_line, start_col, "comentario de bloco nao terminado")
            else:
                break

    def _scan_identifier(self) -> Token:
        line, col = self.line, self.col
        start = self.pos
        while self._peek().isalnum() or self._peek() == "_":
            self._advance()
        text = self.src[start:self.pos]
        ttype = KEYWORDS.get(text, TokenType.ID)
        return Token(ttype, text, line, col)

    def _scan_number(self) -> Token:
        line, col = self.line, self.col
        start = self.pos
        ttype = TokenType.INT_LIT
        while self._peek().isdigit():
            self._advance()
        if self._peek() == "." and self._peek(1).isdigit():
            ttype = TokenType.FLOAT_LIT
            self._advance()
            while self._peek().isdigit():
                self._advance()
        text = self.src[start:self.pos]
        return Token(ttype, text, line, col)

    def _scan_char_literal(self) -> Token:
        line, col = self.line, self.col
        start = self.pos
        self._advance()  # abre aspas simples

        if self._peek() == "'":
            self._advance()
            text = self.src[start:self.pos]
            self._report(line, col, "literal de caractere com tamanho invalido")
            return Token(TokenType.CHAR_LIT, text, line, col)

        if self._at_end() or self._peek() == "\n":
            self._report(line, col, "literal de caractere nao terminado")
            text = self.src[start:self.pos]
            return Token(TokenType.CHAR_LIT, text, line, col)

        if self._peek() == "\\":
            self._advance()
            esc = self._peek()
            if self._at_end() or esc == "\n":
                self._report(line, col, "literal de caractere nao terminado")
                text = self.src[start:self.pos]
                return Token(TokenType.CHAR_LIT, text, line, col)
            if esc not in VALID_ESCAPES:
                self._report(self.line, self.col, f'sequencia de escape desconhecida "\\{esc}"')
            self._advance()
        else:
            self._advance()

        if self._peek() != "'":
            bad_line, bad_col = line, col
            while not self._at_end() and self._peek() != "'" and self._peek() != "\n":
                self._advance()
            if self._peek() == "'":
                self._advance()
                text = self.src[start:self.pos]
                self._report(bad_line, bad_col, "literal de caractere com tamanho invalido")
                return Token(TokenType.CHAR_LIT, text, line, col)
            else:
                text = self.src[start:self.pos]
                self._report(bad_line, bad_col, "literal de caractere nao terminado")
                return Token(TokenType.CHAR_LIT, text, line, col)

        self._advance()  # fecha aspas simples
        text = self.src[start:self.pos]
        return Token(TokenType.CHAR_LIT, text, line, col)

    def _scan_string_literal(self) -> Token:
        line, col = self.line, self.col
        start = self.pos
        self._advance()  # abre aspas duplas

        while not self._at_end() and self._peek() != '"' and self._peek() != "\n":
            if self._peek() == "\\":
                self._advance()
                if self._at_end() or self._peek() == "\n":
                    break
                esc = self._peek()
                if esc not in VALID_ESCAPES:
                    self._report(self.line, self.col, f'sequencia de escape desconhecida "\\{esc}"')
                self._advance()
            else:
                self._advance()

        if self._at_end() or self._peek() != '"':
            self._report(line, col, "literal de cadeia nao terminado")
            text = self.src[start:self.pos]
            return Token(TokenType.STRING_LIT, text, line, col)

        self._advance()  # fecha aspas duplas
        text = self.src[start:self.pos]
        return Token(TokenType.STRING_LIT, text, line, col)

    def next_token(self) -> Token:
        two_char_ops = {
            "==": TokenType.EQ,
            "!=": TokenType.NEQ,
            "<=": TokenType.LE,
            ">=": TokenType.GE,
            "&&": TokenType.AND,
            "||": TokenType.OR,
        }
        one_char_ops = {
            "+": TokenType.PLUS,
            "-": TokenType.MINUS,
            "*": TokenType.STAR,
            "/": TokenType.SLASH,
            "%": TokenType.PERCENT,
            "<": TokenType.LT,
            ">": TokenType.GT,
            "!": TokenType.NOT,
            "=": TokenType.ASSIGN,
            "(": TokenType.LPAREN,
            ")": TokenType.RPAREN,
            "[": TokenType.LBRACKET,
            "]": TokenType.RBRACKET,
            "{": TokenType.LBRACE,
            "}": TokenType.RBRACE,
            ";": TokenType.SEMI,
            ",": TokenType.COMMA,
        }

        while True:
            self._skip_whitespace_and_comments()

            if self._at_end():
                return Token(TokenType.EOF, "", self.line, self.col)

            c = self._peek()
            line, col = self.line, self.col

            if c.isalpha() or c == "_":
                return self._scan_identifier()
            if c.isdigit():
                return self._scan_number()
            if c == "'":
                return self._scan_char_literal()
            if c == '"':
                return self._scan_string_literal()

            two = c + self._peek(1)
            if two in two_char_ops:
                self._advance()
                self._advance()
                return Token(two_char_ops[two], two, line, col)

            if c in one_char_ops:
                self._advance()
                return Token(one_char_ops[c], c, line, col)

            self._report(line, col, f'simbolo "{c}" nao reconhecido')
            self._advance()
            # continua o laco para tentar reconhecer o proximo token

    def tokenize(self) -> List[Token]:
        tokens = []
        while True:
            tok = self.next_token()
            tokens.append(tok)
            if tok.type == TokenType.EOF:
                break
        return tokens
