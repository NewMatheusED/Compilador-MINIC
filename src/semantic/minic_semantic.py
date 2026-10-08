from __future__ import annotations

import os
import sys
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Tuple

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                "..", "parser"))

from minic_ast import (  # noqa: E402
    Assign, Binary, Block, Call, ExprStmt, Function, Id, If, Index, Lit,
    Node, Param, Program, Return, Unary, VarDecl, While,
)


@dataclass(frozen=True)
class Tipo:
    base: str
    vetor: bool = False

    def __str__(self) -> str:
        return self.base + ("[]" if self.vetor else "")

    @property
    def erro(self) -> bool:
        return self.base == "erro"

    @property
    def escalar(self) -> bool:
        return not self.vetor


INT = Tipo("int")
FLOAT = Tipo("float")
BOOL = Tipo("bool")
CHAR = Tipo("char")
VOID = Tipo("void")
STRING = Tipo("string")
ERRO = Tipo("erro")

TIPO_LITERAL = {"int": INT, "real": FLOAT, "bool": BOOL, "char": CHAR,
                "string": STRING}

NUMERICOS = ("int", "float", "char")
INTEIROS = ("int", "char")


def numerico(t: Tipo) -> bool:
    return t.escalar and t.base in NUMERICOS


def inteiro(t: Tipo) -> bool:
    return t.escalar and t.base in INTEIROS


def compativel(destino: Tipo, origem: Tipo) -> bool:
    if destino.erro or origem.erro:
        return True
    if destino.vetor or origem.vetor:
        return destino == origem
    if destino.base == origem.base:
        return True
    if destino.base == "float" and origem.base in ("int", "char"):
        return True
    if destino.base == "int" and origem.base == "char":
        return True
    return False


@dataclass
class Simbolo:
    nome: str
    categoria: str            # variavel | parametro | vetor | funcao
    tipo: Tipo                # na funcao, e o tipo de retorno
    linha: int
    coluna: int
    params: List[Tipo] = field(default_factory=list)

    @property
    def funcao(self) -> bool:
        return self.categoria == "funcao"

    def descrever(self) -> str:
        if self.funcao:
            return f"função {self.tipo}"
        return f"tipo {self.tipo}"


class TabelaSimbolos:
    def __init__(self) -> None:
        self.escopos: List[Dict[str, Simbolo]] = [{}]

    def abrir_escopo(self) -> None:
        self.escopos.append({})

    def fechar_escopo(self) -> None:
        self.escopos.pop()

    def buscar_no_escopo_atual(self, nome: str) -> Optional[Simbolo]:
        return self.escopos[-1].get(nome)

    def buscar(self, nome: str) -> Optional[Simbolo]:
        for escopo in reversed(self.escopos):
            if nome in escopo:
                return escopo[nome]
        return None

    def inserir(self, simbolo: Simbolo) -> None:
        self.escopos[-1][simbolo.nome] = simbolo


PRECEDENCIA = {
    "||": 2, "&&": 3,
    "==": 4, "!=": 4,
    "<": 5, "<=": 5, ">": 5, ">=": 5,
    "+": 6, "-": 6,
    "*": 7, "/": 7, "%": 7,
}
PREC_ATRIB, PREC_UNARIA, PREC_POSFIXA = 1, 8, 9


def _prec(no: Node) -> int:
    if isinstance(no, Assign):
        return PREC_ATRIB
    if isinstance(no, Binary):
        return PRECEDENCIA[no.op]
    if isinstance(no, Unary):
        return PREC_UNARIA
    return PREC_POSFIXA


def texto(no: Node, minimo: int = 0) -> str:
    if isinstance(no, Id):
        s = no.nome
    elif isinstance(no, Lit):
        s = no.valor
    elif isinstance(no, Binary):
        p = PRECEDENCIA[no.op]
        s = f"{texto(no.esq, p)} {no.op} {texto(no.dir, p + 1)}"
    elif isinstance(no, Unary):
        s = f"{no.op}{texto(no.operando, PREC_UNARIA)}"
    elif isinstance(no, Assign):
        s = f"{texto(no.destino, PREC_ATRIB + 1)} = {texto(no.valor, PREC_ATRIB)}"
    elif isinstance(no, Call):
        args = ", ".join(texto(a) for a in no.args)
        s = f"{texto(no.alvo, PREC_POSFIXA)}({args})"
    elif isinstance(no, Index):
        s = f"{texto(no.alvo, PREC_POSFIXA)}[{texto(no.indice)}]"
    else:
        s = "?"
    return f"({s})" if _prec(no) < minimo else s


NOME_LITERAL = {"int": "inteiro", "real": "real", "bool": "booleano",
                "char": "caractere", "string": "string"}


@dataclass
class Diagnostico:
    codigo: str
    linha: int
    coluna: int
    mensagem: str
    ordem: int

    def formatar(self) -> str:
        return (f"{self.codigo} — linha {self.linha}, coluna {self.coluna}: "
                f"{self.mensagem}")


def _aspas(s: str) -> str:
    return f"“{s}”"


class AnalisadorSemantico:
    def __init__(self) -> None:
        self.tabela = TabelaSimbolos()
        self.diagnosticos: List[Diagnostico] = []
        self.funcao_atual: Optional[Function] = None


    def erro(self, codigo: str, no_ou_pos, mensagem: str) -> None:
        if isinstance(no_ou_pos, tuple):
            linha, coluna = no_ou_pos
        else:
            linha, coluna = no_ou_pos.linha, no_ou_pos.coluna
        self.diagnosticos.append(Diagnostico(codigo, linha, coluna, mensagem,
                                             len(self.diagnosticos)))

    def ordenados(self) -> List[Diagnostico]:
        return sorted(self.diagnosticos,
                      key=lambda d: (d.linha, d.coluna, d.ordem))


    def declarar(self, simbolo: Simbolo) -> None:
        anterior = self.tabela.buscar_no_escopo_atual(simbolo.nome)
        if anterior is not None:
            self.erro("SEM002", (simbolo.linha, simbolo.coluna),
                      f"{_aspas(simbolo.nome)} já declarado neste escopo; "
                      f"declaração anterior na linha {anterior.linha}, "
                      f"coluna {anterior.coluna} ({anterior.descrever()}).")
            return
        self.tabela.inserir(simbolo)

    @staticmethod
    def assinatura(f: Function) -> Simbolo:
        params = [Tipo(p.tipo, p.vetor) for p in f.params]
        return Simbolo(f.nome, "funcao", Tipo(f.tipo), f.nome_linha,
                       f.nome_coluna, params)


    def analisar(self, programa: Program) -> List[Diagnostico]:
        self.coletar_assinaturas(programa)
        for item in programa.itens:
            if isinstance(item, Function):
                self.funcao(item)
            elif isinstance(item, VarDecl):
                self.decl_var(item)
            else:
                self.comando(item)
        return self.ordenados()

    def coletar_assinaturas(self, programa: Program) -> None:
        vistos: Dict[str, Simbolo] = {}
        for item in programa.itens:
            if isinstance(item, Function):
                sim = self.assinatura(item)
                anterior = vistos.get(item.nome)
                if anterior is not None:
                    self.erro("SEM002", (sim.linha, sim.coluna),
                              f"{_aspas(sim.nome)} já declarado neste escopo; "
                              f"declaração anterior na linha {anterior.linha}, "
                              f"coluna {anterior.coluna} "
                              f"({anterior.descrever()}).")
                    continue
                vistos[item.nome] = sim
                self.tabela.inserir(sim)
            elif isinstance(item, VarDecl) and item.nome not in vistos:
                vistos[item.nome] = self.simbolo_var(item)

    @staticmethod
    def simbolo_var(d: VarDecl) -> Simbolo:
        vetor = d.tamanho is not None
        return Simbolo(d.nome, "vetor" if vetor else "variavel",
                       Tipo(d.tipo, vetor), d.linha, d.coluna)

    def funcao(self, f: Function) -> None:
        anterior = self.funcao_atual
        self.funcao_atual = f
        self.tabela.abrir_escopo()
        for p in f.params:
            if p.tipo == "void":
                self.erro("SEM014", p,
                          f"Parâmetro {_aspas(p.nome)} não pode ter tipo void.")
            self.declarar(Simbolo(p.nome, "parametro", Tipo(p.tipo, p.vetor),
                                  p.linha, p.coluna))
        # params e corpo no mesmo escopo, igual em C
        for cmd in f.corpo.comandos:
            self.comando(cmd)
        self.tabela.fechar_escopo()

        if f.tipo != "void" and not self.sempre_retorna(f.corpo):
            motivo = self.motivo_queda(f.corpo)
            self.erro("SEM011", f,
                      f"A função {_aspas(f.nome)} pode terminar sem retornar "
                      f"{f.tipo}; {motivo}.")
        self.funcao_atual = anterior

    def decl_var(self, d: VarDecl) -> None:
        sim = self.simbolo_var(d)
        if d.tipo == "void":
            self.erro("SEM014", d,
                      f"Variável {_aspas(d.nome)} não pode ter tipo void.")
            sim.tipo = Tipo("erro", sim.tipo.vetor)

        if d.tamanho is not None:
            t = self.valor(d.tamanho, "tamanho de vetor")
            if not t.erro and not inteiro(t):
                self.erro("SEM006", d.tamanho,
                          f"Tamanho do vetor {_aspas(d.nome)} deve ser int; "
                          f"recebeu {t} (expressão {_aspas(texto(d.tamanho))}).")
            elif (isinstance(d.tamanho, Lit) and d.tamanho.tipo == "int"
                  and d.tamanho.valor.isdigit() and int(d.tamanho.valor) <= 0):
                self.erro("SEM006", d.tamanho,
                          f"Tamanho do vetor {_aspas(d.nome)} deve ser "
                          f"positivo; recebeu {d.tamanho.valor}.")

        if d.init is not None:
            t = self.valor(d.init, "valor de inicialização")
            if not compativel(sim.tipo, t):
                self.erro("SEM003", d.init,
                          f"Não é possível atribuir {t} a {sim.tipo} sem "
                          f"conversão permitida (destino {_aspas(d.nome)}; "
                          f"expressão {_aspas(texto(d.init))}).")

        # so declara depois do init, entao int x = x; da erro
        self.declarar(sim)


    def comando(self, cmd: Node) -> None:
        if isinstance(cmd, VarDecl):
            self.decl_var(cmd)
        elif isinstance(cmd, Block):
            self.tabela.abrir_escopo()
            for c in cmd.comandos:
                self.comando(c)
            self.tabela.fechar_escopo()
        elif isinstance(cmd, If):
            self.condicao(cmd.cond, "if")
            self.comando(cmd.entao)
            if cmd.senao is not None:
                self.comando(cmd.senao)
        elif isinstance(cmd, While):
            self.condicao(cmd.cond, "while")
            self.comando(cmd.corpo)
        elif isinstance(cmd, Return):
            self.retorno(cmd)
        elif isinstance(cmd, ExprStmt):
            self.expr(cmd.expr)

    def condicao(self, cond: Node, comando: str) -> None:
        t = self.valor(cond, "condição")
        if not t.erro and t != BOOL:
            self.erro("SEM005", cond,
                      f"Condição de {comando} deve ter tipo bool; recebeu {t} "
                      f"(expressão {_aspas(texto(cond))}).")

    def retorno(self, r: Return) -> None:
        f = self.funcao_atual
        if f is None:
            if r.valor is not None:
                self.expr(r.valor)
            self.erro("SEM010", r, "Comando return fora de função.")
            return

        esperado = Tipo(f.tipo)
        if r.valor is None:
            if esperado != VOID:
                self.erro("SEM010", r,
                          f"A função {_aspas(f.nome)} deve retornar "
                          f"{esperado}, mas o return não fornece valor.")
            return

        if esperado == VOID:
            self.expr(r.valor)
            self.erro("SEM010", r.valor,
                      f"A função {_aspas(f.nome)} tem retorno void e não pode "
                      f"retornar valor (expressão {_aspas(texto(r.valor))}).")
            return

        t = self.valor(r.valor, "valor de retorno")
        if not compativel(esperado, t):
            self.erro("SEM009", r.valor,
                      f"Retorno {t} incompatível com o tipo {esperado} da "
                      f"função {_aspas(f.nome)}; conversão implícita de {t} "
                      f"para {esperado} não permitida.")


    def sempre_retorna(self, cmd: Optional[Node]) -> bool:
        if isinstance(cmd, Return):
            return True
        if isinstance(cmd, Block):
            return any(self.sempre_retorna(c) for c in cmd.comandos)
        if isinstance(cmd, If):
            return (cmd.senao is not None and self.sempre_retorna(cmd.entao)
                    and self.sempre_retorna(cmd.senao))
        return False

    def motivo_queda(self, cmd: Node) -> str:
        if isinstance(cmd, Block):
            if not cmd.comandos:
                return "o corpo não contém nenhum comando return"
            return self.motivo_queda(cmd.comandos[-1])
        if isinstance(cmd, If):
            cond = _aspas(texto(cmd.cond))
            entao_ok = self.sempre_retorna(cmd.entao)
            if cmd.senao is None:
                if not entao_ok:
                    return self.motivo_queda(cmd.entao)
                return f"o ramo em que {cond} é falso alcança o fim do corpo"
            if not entao_ok and not self.sempre_retorna(cmd.senao):
                return (f"os ramos em que {cond} é verdadeiro e falso "
                        f"alcançam o fim do corpo")
            if not entao_ok:
                return self.motivo_queda(cmd.entao)
            return self.motivo_queda(cmd.senao)
        if isinstance(cmd, While):
            return (f"o laço while com condição {_aspas(texto(cmd.cond))} "
                    f"pode terminar e alcançar o fim do corpo")
        return "o fim do corpo é alcançado sem um comando return"


    def valor(self, no: Node, contexto: str) -> Tipo:
        t = self.expr(no)
        if t == VOID and isinstance(no, Call) and isinstance(no.alvo, Id):
            self.erro("SEM012", no,
                      f"Função {_aspas(no.alvo.nome)} não produz valor "
                      f"(retorno void) e não pode ser usada como {contexto}.")
            return ERRO
        if t == VOID:
            return ERRO
        return t

    def expr(self, no: Node) -> Tipo:
        if isinstance(no, Lit):
            return TIPO_LITERAL[no.tipo]
        if isinstance(no, Id):
            return self.ident(no)
        if isinstance(no, Assign):
            return self.atribuicao(no)
        if isinstance(no, Binary):
            return self.binaria(no)
        if isinstance(no, Unary):
            return self.unaria(no)
        if isinstance(no, Call):
            return self.chamada(no)
        if isinstance(no, Index):
            return self.indexacao(no)
        return ERRO

    def ident(self, no: Id) -> Tipo:
        sim = self.tabela.buscar(no.nome)
        if sim is None:
            self.erro("SEM001", no,
                      f"Identificador {_aspas(no.nome)} não declarado neste "
                      f"escopo.")
            return ERRO
        if sim.funcao:
            self.erro("SEM014", no,
                      f"Função {_aspas(no.nome)} usada como valor; faltam os "
                      f"parênteses da chamada.")
            return ERRO
        return sim.tipo

    def atribuicao(self, no: Assign) -> Tipo:
        destino = self.destino(no.destino)
        t = self.valor(no.valor, "expressão de atribuição")
        if destino is None:
            return ERRO
        if not compativel(destino, t):
            self.erro("SEM003", no.valor,
                      f"Não é possível atribuir {t} a {destino} sem conversão "
                      f"permitida (destino {_aspas(texto(no.destino))}; "
                      f"expressão {_aspas(texto(no.valor))}).")
        return destino

    def destino(self, no: Node) -> Optional[Tipo]:
        if isinstance(no, Id):
            sim = self.tabela.buscar(no.nome)
            if sim is None:
                self.ident(no)
                return None
            if sim.funcao:
                self.erro("SEM013", no,
                          f"Destino de atribuição não é atribuível; a função "
                          f"{_aspas(no.nome)} não designa uma variável ou "
                          f"elemento de vetor.")
                return None
            if sim.tipo.vetor:
                self.erro("SEM013", no,
                          f"Destino de atribuição não é atribuível; o vetor "
                          f"{_aspas(no.nome)} não pode ser atribuído como um "
                          f"todo, apenas seus elementos.")
                return None
            return None if sim.tipo.erro else sim.tipo
        if isinstance(no, Index):
            t = self.indexacao(no)
            return None if t.erro else t

        if isinstance(no, Lit):
            o_que = f"o literal {NOME_LITERAL[no.tipo]} {_aspas(no.valor)}"
        elif isinstance(no, Call):
            o_que = f"a chamada {_aspas(texto(no))}"
        elif isinstance(no, Assign):
            o_que = f"a atribuição {_aspas(texto(no))}"
        else:
            o_que = f"a expressão {_aspas(texto(no))}"
        self.expr(no)
        self.erro("SEM013", no,
                  f"Destino de atribuição não é atribuível; {o_que} não "
                  f"designa uma variável ou elemento de vetor.")
        return None

    def operando_invalido(self, no: Node, op: str, pos, tipos: str) -> None:
        self.erro("SEM004", pos,
                  f"Operador {_aspas(op)} não se aplica a {tipos} "
                  f"(expressão {_aspas(texto(no))}).")

    def binaria(self, no: Binary) -> Tipo:
        te = self.valor(no.esq, "operando")
        td = self.valor(no.dir, "operando")
        pos = (no.op_linha, no.op_coluna)
        op = no.op
        if te.erro or td.erro:
            return BOOL if PRECEDENCIA[op] <= 5 else ERRO

        tipos = f"{te} e {td}"
        if op in ("+", "-", "*", "/"):
            if not (numerico(te) and numerico(td)):
                self.operando_invalido(no, op, pos, tipos)
                return ERRO
            self.divisao_por_zero(no)
            return FLOAT if FLOAT in (te, td) else INT
        if op == "%":
            if not (inteiro(te) and inteiro(td)):
                self.operando_invalido(no, op, pos, tipos)
                return ERRO
            self.divisao_por_zero(no)
            return INT
        if op in ("<", "<=", ">", ">="):
            if not (numerico(te) and numerico(td)):
                self.operando_invalido(no, op, pos, tipos)
            return BOOL
        if op in ("==", "!="):
            ok = ((numerico(te) and numerico(td))
                  or (te == td and te.escalar and te.base == "bool"))
            if not ok:
                self.operando_invalido(no, op, pos, tipos)
            return BOOL
        if te != BOOL or td != BOOL:
            self.operando_invalido(no, op, pos, tipos)
        return BOOL

    def divisao_por_zero(self, no: Binary) -> None:
        if no.op not in ("/", "%"):
            return
        d = no.dir
        if isinstance(d, Lit) and d.tipo in ("int", "real"):
            try:
                zero = float(d.valor) == 0.0
            except ValueError:
                zero = False
            if zero:
                self.erro("SEM015", d,
                          f"Divisor constante zero em {_aspas(texto(no))}.")

    def unaria(self, no: Unary) -> Tipo:
        t = self.valor(no.operando, "operando")
        if t.erro:
            return BOOL if no.op == "!" else ERRO
        if no.op == "!":
            if t != BOOL:
                self.operando_invalido(no, "!", no, str(t))
            return BOOL
        if not numerico(t):
            self.operando_invalido(no, no.op, no, str(t))
            return ERRO
        return INT if t.base == "char" else t

    def chamada(self, no: Call) -> Tipo:
        if not isinstance(no.alvo, Id):
            self.expr(no.alvo)
            for a in no.args:
                self.expr(a)
            self.erro("SEM014", no,
                      f"A expressão {_aspas(texto(no.alvo))} não é uma função "
                      f"e não pode ser chamada.")
            return ERRO

        nome = no.alvo.nome
        sim = self.tabela.buscar(nome)
        if sim is None or not sim.funcao:
            for a in no.args:
                self.expr(a)
            if sim is None:
                self.erro("SEM001", no.alvo,
                          f"Identificador {_aspas(nome)} não declarado neste "
                          f"escopo.")
            else:
                self.erro("SEM014", no.alvo,
                          f"{_aspas(nome)} não é uma função ({sim.descrever()}) "
                          f"e não pode ser chamado.")
            return ERRO

        tipos_args = [self.valor(a, "argumento") for a in no.args]
        esperado, recebido = len(sim.params), len(no.args)
        if esperado != recebido:
            plural = "argumento" if esperado == 1 else "argumentos"
            self.erro("SEM007", no,
                      f"{_aspas(nome)} espera {esperado} {plural}, mas "
                      f"recebeu {recebido}.")
        else:
            for i, (pt, at, arg) in enumerate(zip(sim.params, tipos_args,
                                                  no.args), start=1):
                if not compativel(pt, at):
                    self.erro("SEM008", arg,
                              f"Argumento {i} de {_aspas(nome)}: esperado "
                              f"{pt}, recebido {at} (expressão "
                              f"{_aspas(texto(arg))}).")
        return sim.tipo

    def indexacao(self, no: Index) -> Tipo:
        tb = self.expr(no.alvo)
        ti = self.valor(no.indice, "índice")
        nome = texto(no.alvo)
        if not tb.erro and not tb.vetor:
            self.erro("SEM006", no.alvo,
                      f"{_aspas(nome)} não é um vetor ({tb}) e não pode ser "
                      f"indexado.")
            tb = ERRO
        if not ti.erro and not inteiro(ti):
            self.erro("SEM006", no.indice,
                      f"Índice do vetor {_aspas(nome)} deve ser int; recebeu "
                      f"{ti} (expressão {_aspas(texto(no.indice))}).")
        if tb.erro:
            return ERRO
        return Tipo(tb.base)


# os .gabarito do professor usam CRLF e nao tem \n no final
SEPARADOR = "\r\n"


def resumo(n: int) -> str:
    palavra = "erro" if n == 1 else "erros"
    estado = "aceito" if n == 0 else "rejeitado"
    return f"Análise semântica concluída: {n} {palavra}; programa {estado}."


def analisar(programa: Program) -> Tuple[List[Diagnostico], str]:
    diags = AnalisadorSemantico().analisar(programa)
    linhas = [d.formatar() for d in diags] + [resumo(len(diags))]
    return diags, SEPARADOR.join(linhas)
