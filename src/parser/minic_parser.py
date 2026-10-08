"""Analisador sintatico descendente recursivo do MINIC (etapa 2).

O parser consome tokens sob demanda do Lexer da etapa 1 (nao existe lista
intermediaria nem arquivo de tokens): um buffer pequeno de lookahead pede
next_token() apenas quando precisa olhar mais a frente.

Gramatica reconhecida:

    programa    -> item*
    item        -> funcao | decl_var | comando
    funcao      -> TIPO IDENT '(' params ')' bloco
    params      -> vazio | param (',' param)*
    param       -> TIPO IDENT ('[' ']')?
    decl_var    -> TIPO IDENT ('[' expr ']')? ('=' expr)? ';'
    comando     -> bloco | se | enquanto | retorno | decl_var | expr_cmd
    bloco       -> '{' comando* '}'
    se          -> 'if' '(' expr ')' comando ('else' comando)?
    enquanto    -> 'while' '(' expr ')' comando
    retorno     -> 'return' expr? ';'
    expr_cmd    -> expr ';'

    expr        -> atribuicao
    atribuicao  -> ou ('=' atribuicao)?        (associativa a direita)
    ou          -> e ('||' e)*
    e           -> igualdade ('&&' igualdade)*
    igualdade   -> relacional (('=='|'!=') relacional)*
    relacional  -> aditiva (('<'|'<='|'>'|'>=') aditiva)*
    aditiva     -> multiplicativa (('+'|'-') multiplicativa)*
    multipl.    -> unaria (('*'|'/'|'%') unaria)*
    unaria      -> ('-'|'!'|'+') unaria | posfixa
    posfixa     -> primaria ('(' args ')' | '[' expr ']')*
    primaria    -> IDENT | literal | 'true' | 'false' | '(' expr ')'

Observacoes que vieram dos 50 casos de teste:

  - o nivel global aceita comandos, nao so declaracoes (caso 22);
  - funcao sem corpo (prototipo) e erro (caso 48);
  - '=' aceita qualquer lado esquerdo, o semantico que barra `3 = n;`
    (SEM013). O caso 41 continua dando "esperado FECHA_COLCHETE".
"""

from __future__ import annotations

import os
import sys
from typing import List, Optional

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                "..", "lexer"))

from lexer import Lexer, Token, TokenType  # noqa: E402

from minic_ast import (  # noqa: E402
    Assign, Binary, Block, Call, ExprStmt, Function, Id, If, Index, Lit,
    Node, Param, Program, Return, Unary, VarDecl, While,
)


# --------------------------------------------------------------------------
# Diagnostico
# --------------------------------------------------------------------------

NOMES_TOKEN = {
    TokenType.ID: "IDENT",
    TokenType.INT_LIT: "LITERAL_INT",
    TokenType.FLOAT_LIT: "LITERAL_REAL",
    TokenType.CHAR_LIT: "LITERAL_CHAR",
    TokenType.STRING_LIT: "LITERAL_STRING",
    TokenType.LPAREN: "ABRE_PAREN",
    TokenType.RPAREN: "FECHA_PAREN",
    TokenType.LBRACE: "ABRE_CHAVE",
    TokenType.RBRACE: "FECHA_CHAVE",
    TokenType.LBRACKET: "ABRE_COLCHETE",
    TokenType.RBRACKET: "FECHA_COLCHETE",
    TokenType.SEMI: "PONTO_E_VIRGULA",
    TokenType.COMMA: "VIRGULA",
    TokenType.ASSIGN: "ATRIBUICAO",
    TokenType.EOF: "FIM_DE_ARQUIVO",
}


def nome_token(tipo: TokenType) -> str:
    return NOMES_TOKEN.get(tipo, tipo.name)


class ErroSintatico(Exception):
    def __init__(self, mensagem: str, linha: int, coluna: int):
        super().__init__(mensagem)
        self.mensagem = mensagem
        self.linha = linha
        self.coluna = coluna

    def formatar(self) -> str:
        return (f"Erro sintatico na linha {self.linha}, "
                f"coluna {self.coluna}: {self.mensagem}.")


class ErroLexicoFatal(ErroSintatico):
    """Erro lexico encontrado durante a analise sintatica."""


TIPOS = {
    TokenType.KW_INT: "int",
    TokenType.KW_FLOAT: "float",
    TokenType.KW_BOOL: "bool",
    TokenType.KW_CHAR: "char",
    TokenType.KW_VOID: "void",
}

NOMES_TIPOS = "KW_INT, KW_FLOAT, KW_BOOL, KW_CHAR ou KW_VOID"

INICIO_COMANDO = {
    TokenType.LBRACE, TokenType.KW_IF, TokenType.KW_WHILE,
    TokenType.KW_RETURN, TokenType.ID, TokenType.INT_LIT,
    TokenType.FLOAT_LIT, TokenType.CHAR_LIT, TokenType.STRING_LIT,
    TokenType.KW_TRUE, TokenType.KW_FALSE, TokenType.LPAREN,
    TokenType.MINUS, TokenType.PLUS, TokenType.NOT, TokenType.SEMI,
}

TIPOS_LITERAL = {
    TokenType.INT_LIT: "int",
    TokenType.FLOAT_LIT: "real",
    TokenType.CHAR_LIT: "char",
    TokenType.STRING_LIT: "string",
    TokenType.KW_TRUE: "bool",
    TokenType.KW_FALSE: "bool",
}

OPS_IGUALDADE = {TokenType.EQ: "==", TokenType.NEQ: "!="}
OPS_RELACIONAL = {TokenType.LT: "<", TokenType.LE: "<=",
                  TokenType.GT: ">", TokenType.GE: ">="}
OPS_ADITIVO = {TokenType.PLUS: "+", TokenType.MINUS: "-"}
OPS_MULTIPLICATIVO = {TokenType.STAR: "*", TokenType.SLASH: "/",
                      TokenType.PERCENT: "%"}
OPS_UNARIO = {TokenType.MINUS: "-", TokenType.NOT: "!", TokenType.PLUS: "+"}


# --------------------------------------------------------------------------
# Parser
# --------------------------------------------------------------------------

class Parser:
    def __init__(self, fonte: str):
        self._erros_lexicos: List[str] = []

        def registrar(msg, linha, coluna):
            self._erros_lexicos.append(
                f"Erro lexico na linha {linha}, coluna {coluna}: {msg}.")

        self.lexer = Lexer(fonte, on_error=registrar)
        self._buffer: List[Token] = []

    # ---- acesso aos tokens (sob demanda) ---------------------------------

    def _encher(self, quantos: int) -> None:
        while len(self._buffer) < quantos:
            self._buffer.append(self.lexer.next_token())

    def espiar(self, adiante: int = 0) -> Token:
        self._encher(adiante + 1)
        return self._buffer[adiante]

    def avancar(self) -> Token:
        self._encher(1)
        return self._buffer.pop(0)

    def conferir(self, tipo: TokenType) -> bool:
        return self.espiar().type == tipo

    def aceitar(self, tipo: TokenType) -> Optional[Token]:
        if self.conferir(tipo):
            return self.avancar()
        return None

    def exigir(self, tipo: TokenType, esperado: Optional[str] = None) -> Token:
        if self.conferir(tipo):
            return self.avancar()
        self.erro(f"esperado {esperado or nome_token(tipo)}")

    def erro(self, mensagem: str) -> None:
        tok = self.espiar()
        encontrado = nome_token(tok.type)
        if tok.type != TokenType.EOF:
            encontrado += f' ("{tok.lexeme}")'
        raise ErroSintatico(f"{mensagem}, encontrado {encontrado}",
                            tok.line, tok.col)

    # ---- ponto de entrada ------------------------------------------------

    def analisar(self) -> Program:
        itens: List[Node] = []
        while not self.conferir(TokenType.EOF):
            itens.append(self.item())
        self._checar_erros_lexicos()
        return Program(itens)

    def _checar_erros_lexicos(self) -> None:
        if self._erros_lexicos:
            raise ErroLexicoFatal(self._erros_lexicos[0], 0, 0)

    # ---- nivel global ----------------------------------------------------

    def item(self) -> Node:
        tok = self.espiar()

        if tok.type in TIPOS:
            # TIPO IDENT '('  -> funcao;  caso contrario, declaracao
            if (self.espiar(1).type == TokenType.ID
                    and self.espiar(2).type == TokenType.LPAREN):
                return self.funcao()
            return self.decl_var()

        if tok.type == TokenType.RBRACE:
            self.erro("token FECHA_CHAVE inesperado no nivel global")
        if tok.type == TokenType.KW_ELSE:
            self.erro("token KW_ELSE inesperado (nao ha if correspondente)")

        return self.comando()

    def funcao(self) -> Function:
        tok_tipo = self.avancar()
        tipo = TIPOS[tok_tipo.type]
        tok_nome = self.exigir(TokenType.ID, "IDENT")
        nome = tok_nome.lexeme
        self.exigir(TokenType.LPAREN, "ABRE_PAREN")

        params: List[Param] = []
        if not self.conferir(TokenType.RPAREN):
            params.append(self.param())
            while self.aceitar(TokenType.COMMA):
                params.append(self.param(depois_de_virgula=True))

        if not self.conferir(TokenType.RPAREN):
            self.erro("esperado VIRGULA ou FECHA_PAREN")
        self.avancar()

        if not self.conferir(TokenType.LBRACE):
            # `int f();` -- prototipo nao faz parte do subconjunto
            self.erro("esperado ABRE_CHAVE (funcao precisa de corpo)")
        corpo = self.bloco()
        no = Function(tipo, nome, params, corpo)
        no.nome_linha, no.nome_coluna = tok_nome.line, tok_nome.col
        return no.em(tok_tipo.line, tok_tipo.col)

    def param(self, depois_de_virgula: bool = False) -> Param:
        tok = self.espiar()
        if tok.type not in TIPOS:
            if depois_de_virgula:
                self.erro(f"esperado tipo ({NOMES_TIPOS}) ou FECHA_PAREN")
            self.erro(f"esperado tipo ({NOMES_TIPOS})")
        tipo = TIPOS[self.avancar().type]
        tok_nome = self.exigir(TokenType.ID, "IDENT")
        vetor = False
        if self.aceitar(TokenType.LBRACKET):
            self.exigir(TokenType.RBRACKET, "FECHA_COLCHETE")
            vetor = True
        return Param(tipo, tok_nome.lexeme, vetor).em(tok_nome.line,
                                                      tok_nome.col)

    def decl_var(self) -> VarDecl:
        tipo = TIPOS[self.avancar().type]
        tok_nome = self.exigir(TokenType.ID, "IDENT")
        nome = tok_nome.lexeme

        tamanho = None
        if self.aceitar(TokenType.LBRACKET):
            if self.conferir(TokenType.RBRACKET):
                self.erro("esperado expressao no tamanho do vetor")
            tamanho = self.expr()
            self.exigir(TokenType.RBRACKET, "FECHA_COLCHETE")

        init = None
        if self.aceitar(TokenType.ASSIGN):
            if self.conferir(TokenType.SEMI):
                self.erro("esperado expressao no inicializador")
            init = self.expr()

        self.exigir(TokenType.SEMI, "PONTO_E_VIRGULA")
        return VarDecl(tipo, nome, tamanho, init).em(tok_nome.line,
                                                      tok_nome.col)

    # ---- comandos --------------------------------------------------------

    def comando(self) -> Node:
        tok = self.espiar()

        if tok.type in TIPOS:
            return self.decl_var()
        if tok.type == TokenType.LBRACE:
            return self.bloco()
        if tok.type == TokenType.KW_IF:
            return self.comando_if()
        if tok.type == TokenType.KW_WHILE:
            return self.comando_while()
        if tok.type == TokenType.KW_RETURN:
            return self.comando_return()
        if tok.type == TokenType.KW_ELSE:
            self.erro("token KW_ELSE inesperado (nao ha if correspondente)")
        if tok.type in (TokenType.RBRACE, TokenType.EOF):
            self.erro("esperado inicio de statement")

        expr = self.expr()
        self.exigir(TokenType.SEMI, "PONTO_E_VIRGULA")
        return ExprStmt(expr).em(expr.linha, expr.coluna)

    def bloco(self) -> Block:
        abre = self.exigir(TokenType.LBRACE, "ABRE_CHAVE")
        comandos: List[Node] = []
        while not self.conferir(TokenType.RBRACE):
            if self.conferir(TokenType.EOF):
                self.erro("esperado FECHA_CHAVE")
            comandos.append(self.comando())
        self.avancar()
        return Block(comandos).em(abre.line, abre.col)

    def comando_if(self) -> If:
        kw = self.avancar()
        self.exigir(TokenType.LPAREN, "ABRE_PAREN")
        if self.conferir(TokenType.RPAREN):
            self.erro("esperado expressao na condicao")
        cond = self.expr()
        self.exigir(TokenType.RPAREN, "FECHA_PAREN")
        entao = self.comando()
        senao = self.comando() if self.aceitar(TokenType.KW_ELSE) else None
        return If(cond, entao, senao).em(kw.line, kw.col)

    def comando_while(self) -> While:
        kw = self.avancar()
        self.exigir(TokenType.LPAREN, "ABRE_PAREN")
        if self.conferir(TokenType.RPAREN):
            self.erro("esperado expressao na condicao")
        cond = self.expr()
        self.exigir(TokenType.RPAREN, "FECHA_PAREN")
        return While(cond, self.comando()).em(kw.line, kw.col)

    def comando_return(self) -> Return:
        kw = self.avancar()
        if self.aceitar(TokenType.SEMI):
            return Return(None).em(kw.line, kw.col)
        valor = self.expr()
        self.exigir(TokenType.SEMI, "PONTO_E_VIRGULA")
        return Return(valor).em(kw.line, kw.col)

    # ---- expressoes ------------------------------------------------------

    def expr(self) -> Node:
        return self.atribuicao()

    def atribuicao(self) -> Node:
        esq = self.logico_ou()
        if self.aceitar(TokenType.ASSIGN):
            return Assign(esq, self.atribuicao()).em(esq.linha, esq.coluna)
        return esq

    def _binaria(self, proximo, operadores):
        no = proximo()
        while self.espiar().type in operadores:
            tok_op = self.avancar()
            op = operadores[tok_op.type]
            no = Binary(op, no, proximo()).em(no.linha, no.coluna)
            no.op_linha, no.op_coluna = tok_op.line, tok_op.col
        return no

    def logico_ou(self) -> Node:
        return self._binaria(self.logico_e, {TokenType.OR: "||"})

    def logico_e(self) -> Node:
        return self._binaria(self.igualdade, {TokenType.AND: "&&"})

    def igualdade(self) -> Node:
        return self._binaria(self.relacional, OPS_IGUALDADE)

    def relacional(self) -> Node:
        return self._binaria(self.aditiva, OPS_RELACIONAL)

    def aditiva(self) -> Node:
        return self._binaria(self.multiplicativa, OPS_ADITIVO)

    def multiplicativa(self) -> Node:
        return self._binaria(self.unaria, OPS_MULTIPLICATIVO)

    def unaria(self) -> Node:
        tipo = self.espiar().type
        if tipo in OPS_UNARIO:
            tok_op = self.avancar()
            op = OPS_UNARIO[tok_op.type]
            return Unary(op, self.unaria()).em(tok_op.line, tok_op.col)
        return self.posfixa()

    def posfixa(self) -> Node:
        no = self.primaria()
        while True:
            if self.aceitar(TokenType.LPAREN):
                args: List[Node] = []
                if not self.conferir(TokenType.RPAREN):
                    args.append(self.expr())
                    while self.aceitar(TokenType.COMMA):
                        if self.conferir(TokenType.RPAREN):
                            self.erro("esperado expressao ou FECHA_PAREN")
                        args.append(self.expr())
                self.exigir(TokenType.RPAREN, "FECHA_PAREN")
                no = Call(no, args).em(no.linha, no.coluna)
            elif self.aceitar(TokenType.LBRACKET):
                if self.conferir(TokenType.RBRACKET):
                    self.erro("esperado expressao no indice")
                indice = self.expr()
                self.exigir(TokenType.RBRACKET, "FECHA_COLCHETE")
                no = Index(no, indice).em(no.linha, no.coluna)
            else:
                return no

    def primaria(self) -> Node:
        tok = self.espiar()

        if tok.type == TokenType.ID:
            return Id(self.avancar().lexeme).em(tok.line, tok.col)
        if tok.type in TIPOS_LITERAL:
            tipo = TIPOS_LITERAL[tok.type]
            return Lit(tipo, self.avancar().lexeme).em(tok.line, tok.col)
        if self.aceitar(TokenType.LPAREN):
            if self.conferir(TokenType.RPAREN):
                self.erro("esperado expressao")
            no = self.expr()
            self.exigir(TokenType.RPAREN, "FECHA_PAREN")
            return no

        self.erro("esperado identificador, literal ou ABRE_PAREN")


def analisar_fonte(fonte: str) -> Program:
    return Parser(fonte).analisar()
