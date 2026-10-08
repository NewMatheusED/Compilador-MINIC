"""Nos da arvore sintatica abstrata (AST) do MINIC.

A representacao textual segue a notacao de S-expressao descrita no README
do conjunto de testes da etapa 2:

    Program, Function, Block, VarDecl, If, While, Return, ExprStmt,
    Assign, Binary, Unary, Call, Index, Id, Lit

Convencao de espacamento adotada (a mesma usada na maioria dos gabaritos):

  - filhos de Program e de Block sao separados por ", " (virgula + espaco),
    porque sao listas de comandos;
  - filhos de qualquer outro no sao separados por "," (sem espaco);
  - o inicializador de VarDecl aparece como "=" sem espacos em volta.

Alguns arquivos ast.esperada.txt do professor usam espacamento diferente
(ex.: "int x = Lit(int,42)" em vez de "int x=Lit(int,42)"). Por isso o
script tests/etapa2/testar_parser.sh compara ignorando espacos em branco.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import List, Optional


class Node:
    linha: int = 0
    coluna: int = 0

    def em(self, linha: int, coluna: int) -> "Node":
        self.linha = linha
        self.coluna = coluna
        return self

    def render(self) -> str:  # pragma: no cover - sobrescrito nas subclasses
        raise NotImplementedError

    def __str__(self) -> str:
        return self.render()


def _lista_comandos(nos: List[Node]) -> str:
    return ", ".join(n.render() for n in nos)


def _lista_expr(nos: List[Node]) -> str:
    return ",".join(n.render() for n in nos)


# --------------------------------------------------------------------------
# Expressoes
# --------------------------------------------------------------------------

@dataclass
class Id(Node):
    nome: str

    def render(self) -> str:
        return f"Id({self.nome})"


@dataclass
class Lit(Node):
    tipo: str      # int | real | bool | char | string
    valor: str

    def render(self) -> str:
        return f"Lit({self.tipo},{self.valor})"


@dataclass
class Binary(Node):
    op: str
    esq: Node
    dir: Node

    op_linha: int = field(default=0, compare=False, repr=False)
    op_coluna: int = field(default=0, compare=False, repr=False)

    def render(self) -> str:
        return f"Binary({self.op},{self.esq.render()},{self.dir.render()})"


@dataclass
class Unary(Node):
    op: str
    operando: Node

    def render(self) -> str:
        return f"Unary({self.op},{self.operando.render()})"


@dataclass
class Assign(Node):
    destino: Node
    valor: Node

    def render(self) -> str:
        return f"Assign({self.destino.render()},{self.valor.render()})"


@dataclass
class Call(Node):
    alvo: Node
    args: List[Node] = field(default_factory=list)

    def render(self) -> str:
        partes = [self.alvo.render()] + [a.render() for a in self.args]
        return f"Call({','.join(partes)})"


@dataclass
class Index(Node):
    alvo: Node
    indice: Node

    def render(self) -> str:
        return f"Index({self.alvo.render()},{self.indice.render()})"


# --------------------------------------------------------------------------
# Comandos e declaracoes
# --------------------------------------------------------------------------

@dataclass
class VarDecl(Node):
    tipo: str
    nome: str
    tamanho: Optional[Node] = None   # vetor: int v[3];
    init: Optional[Node] = None      # inicializador: int x = 1;

    def render(self) -> str:
        texto = f"{self.tipo} {self.nome}"
        if self.tamanho is not None:
            texto += f" size={self.tamanho.render()}"
        if self.init is not None:
            texto += f"={self.init.render()}"
        return f"VarDecl({texto})"


@dataclass
class Param(Node):
    tipo: str
    nome: str
    vetor: bool = False

    def render(self) -> str:
        sufixo = "[]" if self.vetor else ""
        return f"{self.tipo} {self.nome}{sufixo}"


@dataclass
class Block(Node):
    comandos: List[Node] = field(default_factory=list)

    def render(self) -> str:
        return f"Block({_lista_comandos(self.comandos)})"


@dataclass
class Function(Node):
    tipo: str
    nome: str
    params: List[Param]
    corpo: Block

    nome_linha: int = field(default=0, compare=False, repr=False)
    nome_coluna: int = field(default=0, compare=False, repr=False)

    def render(self) -> str:
        params = ",".join(p.render() for p in self.params)
        return f"Function({self.tipo} {self.nome}({params}) {self.corpo.render()})"


@dataclass
class If(Node):
    cond: Node
    entao: Node
    senao: Optional[Node] = None

    def render(self) -> str:
        senao = self.senao.render() if self.senao is not None else "NULL"
        return f"If({self.cond.render()},{self.entao.render()},{senao})"


@dataclass
class While(Node):
    cond: Node
    corpo: Node

    def render(self) -> str:
        return f"While({self.cond.render()},{self.corpo.render()})"


@dataclass
class Return(Node):
    valor: Optional[Node] = None

    def render(self) -> str:
        valor = self.valor.render() if self.valor is not None else "NULL"
        return f"Return({valor})"


@dataclass
class ExprStmt(Node):
    expr: Node

    def render(self) -> str:
        return f"ExprStmt({self.expr.render()})"


@dataclass
class Program(Node):
    itens: List[Node] = field(default_factory=list)

    def render(self) -> str:
        return f"Program({_lista_comandos(self.itens)})"
