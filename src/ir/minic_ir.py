from __future__ import annotations

import os
import re
import sys
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Tuple

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                "..", "parser"))

from minic_ast import (  # noqa: E402
    Assign, Binary, Block, Break, Call, Continue, ExprStmt, For, Function,
    Id, If, Index, Lit, Node, Print, Program, Read, Return, Unary, VarDecl,
    While,
)

# Formato de cada instrucao (campos usados):
#   copy    dst = a              bin   dst = a opr b       un  dst = opr a
#   load    dst = load a[b]      store store a[b], c
#   label   opr:                 goto  goto opr
#   iffalse ifFalse a goto opr   iftrue ifTrue a goto opr
#   param   param a              call  [dst =] call opr, n
#   return  return [a]           print print a              read read dst
#   array   array dst[a]         global global dst[[a]]


@dataclass
class Instr:
    op: str
    dst: Optional[str] = None
    a: Optional[str] = None
    b: Optional[str] = None
    c: Optional[str] = None
    opr: Optional[str] = None
    n: int = 0

    def texto(self) -> str:
        op = self.op
        if op == "copy":
            return f"{self.dst} = {self.a}"
        if op == "bin":
            return f"{self.dst} = {self.a} {self.opr} {self.b}"
        if op == "un":
            return f"{self.dst} = {self.opr} {self.a}"
        if op == "load":
            return f"{self.dst} = load {self.a}[{self.b}]"
        if op == "store":
            return f"store {self.a}[{self.b}], {self.c}"
        if op == "label":
            return f"{self.opr}:"
        if op == "goto":
            return f"goto {self.opr}"
        if op == "iffalse":
            return f"ifFalse {self.a} goto {self.opr}"
        if op == "iftrue":
            return f"ifTrue {self.a} goto {self.opr}"
        if op == "param":
            return f"param {self.a}"
        if op == "call":
            chamada = f"call {self.opr}, {self.n}"
            return f"{self.dst} = {chamada}" if self.dst else chamada
        if op == "return":
            return f"return {self.a}" if self.a is not None else "return"
        if op == "print":
            return f"print {self.a}"
        if op == "read":
            return f"read {self.dst}"
        if op == "array":
            return f"array {self.dst}[{self.a}]"
        if op == "global":
            if self.a is not None:
                return f"global {self.dst}[{self.a}]"
            return f"global {self.dst}"
        raise ValueError(op)


@dataclass
class Unidade:
    nome: str
    params: List[str] = field(default_factory=list)
    instrs: List[Instr] = field(default_factory=list)


@dataclass
class ProgramaIR:
    globais: List[str] = field(default_factory=list)
    init: Unidade = field(default_factory=lambda: Unidade("init"))
    funcoes: List[Unidade] = field(default_factory=list)

    def texto(self) -> str:
        linhas: List[str] = []
        if self.init.instrs:
            linhas.append("init")
            linhas += [_linha(i) for i in self.init.instrs]
            linhas.append("endinit")
        for f in self.funcoes:
            linhas.append(f"func {f.nome}({', '.join(f.params)})"
                          if f.params else f"func {f.nome}")
            linhas += [_linha(i) for i in f.instrs]
            linhas.append("endfunc")
        return "".join(l + "\n" for l in linhas)


def _linha(i: Instr) -> str:
    return i.texto() if i.op == "label" else "  " + i.texto()


TEMP = re.compile(r"t[0-9]+$")
INT_LIT = re.compile(r"-?[0-9]+$")

OPS_BIN = {"&&": "and", "||": "or"}
OPS_UN = {"-": "neg", "!": "not"}


@dataclass
class Var:
    nome_ir: str
    tipo: str
    vetor: bool


class Gerador:
    def __init__(self) -> None:
        self.prog = ProgramaIR()
        self.funcoes: Dict[str, Tuple[str, List[Tuple[str, bool]]]] = {}
        self.globais: Dict[str, Var] = {}
        self.unidade = self.prog.init
        self.escopos: List[Dict[str, Var]] = []
        self.usados: set = set()
        self.ntemp = 0
        self.rotulos: Dict[str, int] = {}
        self.lacos: List[List] = []
        self.tipo_ret = "void"

    def gerar(self, programa: Program) -> ProgramaIR:
        for item in programa.itens:
            if isinstance(item, Function):
                params = [(p.tipo, p.vetor) for p in item.params]
                self.funcoes.setdefault(item.nome, (item.tipo, params))

        self.escopos = [self.globais]
        for item in programa.itens:
            if not isinstance(item, Function):
                self.comando(item, global_=True)

        for item in programa.itens:
            if isinstance(item, Function):
                self.funcao(item)
        return self.prog

    # ---- nomes ---------------------------------------------------------

    def novo_nome(self, base: str) -> str:
        if TEMP.match(base) is None and base not in self.usados:
            nome = base
        else:
            k = 1 if TEMP.match(base) else 2
            while f"{base}#{k}" in self.usados:
                k += 1
            nome = f"{base}#{k}"
        self.usados.add(nome)
        return nome

    def temp(self) -> str:
        self.ntemp += 1
        return f"t{self.ntemp}"

    def rotulo(self, base: str) -> str:
        k = self.rotulos.get(base, 0) + 1
        self.rotulos[base] = k
        return base if k == 1 else f"{base}{k}"

    def buscar(self, nome: str) -> Var:
        for escopo in reversed(self.escopos):
            if nome in escopo:
                return escopo[nome]
        raise KeyError(nome)

    def emitir(self, op: str, **campos) -> Instr:
        i = Instr(op, **campos)
        self.unidade.instrs.append(i)
        return i

    # ---- tipos ---------------------------------------------------------

    def tipo(self, no: Node) -> str:
        if isinstance(no, Lit):
            return {"real": "float"}.get(no.tipo, no.tipo)
        if isinstance(no, Id):
            return self.buscar(no.nome).tipo
        if isinstance(no, Index):
            return self.tipo(no.alvo)
        if isinstance(no, Assign):
            return self.tipo(no.destino)
        if isinstance(no, Call):
            return self.funcoes[no.alvo.nome][0]
        if isinstance(no, Unary):
            if no.op == "!":
                return "bool"
            return "float" if self.tipo(no.operando) == "float" else "int"
        if isinstance(no, Binary):
            if no.op in ("+", "-", "*", "/"):
                floats = "float" in (self.tipo(no.esq), self.tipo(no.dir))
                return "float" if floats else "int"
            if no.op == "%":
                return "int"
            return "bool"
        return "int"

    def converter(self, valor: str, de: str, para: str) -> str:
        if para != "float" or de not in ("int", "char"):
            return valor
        if INT_LIT.match(valor):
            return valor + ".0"
        t = self.temp()
        self.emitir("un", dst=t, opr="itof", a=valor)
        return t

    # ---- funcoes e comandos -------------------------------------------

    def funcao(self, f: Function) -> None:
        self.unidade = Unidade(f.nome)
        self.prog.funcoes.append(self.unidade)
        self.usados = set(v.nome_ir for v in self.globais.values())
        self.ntemp = 0
        self.rotulos = {}
        self.tipo_ret = f.tipo
        escopo: Dict[str, Var] = {}
        self.escopos = [self.globais, escopo]
        for p in f.params:
            nome = self.novo_nome(p.nome)
            escopo[p.nome] = Var(nome, p.tipo, p.vetor)
            self.unidade.params.append(nome + "[]" if p.vetor else nome)
        for cmd in f.corpo.comandos:
            self.comando(cmd)
        fim = self.unidade.instrs[-1] if self.unidade.instrs else None
        if f.tipo == "void" and (fim is None or fim.op != "return"):
            self.emitir("return")

    def comando(self, cmd: Node, global_: bool = False) -> None:
        if isinstance(cmd, VarDecl):
            self.decl_var(cmd, global_)
        elif isinstance(cmd, Block):
            self.escopos.append({})
            for c in cmd.comandos:
                self.comando(c)
            self.escopos.pop()
        elif isinstance(cmd, If):
            self.comando_if(cmd)
        elif isinstance(cmd, While):
            self.laco(cmd.cond, cmd.corpo, None)
        elif isinstance(cmd, For):
            if cmd.init is not None:
                self.expr(cmd.init)
            self.laco(cmd.cond, cmd.corpo, cmd.passo)
        elif isinstance(cmd, Break):
            self.emitir("goto", opr=self.lacos[-1][0])
        elif isinstance(cmd, Continue):
            ctx = self.lacos[-1]
            if ctx[1] is None:
                ctx[1] = self.rotulo("L_step")
            self.emitir("goto", opr=ctx[1])
        elif isinstance(cmd, Return):
            if cmd.valor is None:
                self.emitir("return")
            else:
                v = self.expr(cmd.valor)
                v = self.converter(v, self.tipo(cmd.valor), self.tipo_ret)
                self.emitir("return", a=v)
        elif isinstance(cmd, Print):
            self.emitir("print", a=self.expr(cmd.valor))
        elif isinstance(cmd, Read):
            self.comando_read(cmd.destino)
        elif isinstance(cmd, ExprStmt):
            self.expr(cmd.expr)

    def decl_var(self, d: VarDecl, global_: bool) -> None:
        tamanho = self.expr(d.tamanho) if d.tamanho is not None else None
        valor = self.expr(d.init) if d.init is not None else None
        nome = self.novo_nome(d.nome)
        self.escopos[-1][d.nome] = Var(nome, d.tipo, tamanho is not None)
        if global_:
            self.prog.globais.append(nome)
            self.emitir("global", dst=nome, a=tamanho)
        elif tamanho is not None:
            self.emitir("array", dst=nome, a=tamanho)
        if valor is not None:
            valor = self.converter(valor, self.tipo(d.init), d.tipo)
            self.emitir("copy", dst=nome, a=valor)

    def comando_if(self, cmd: If) -> None:
        cond = self.expr(cmd.cond)
        if cmd.senao is None:
            fim = self.rotulo("L_end_if")
            self.emitir("iffalse", a=cond, opr=fim)
            self.comando(cmd.entao)
            self.emitir("label", opr=fim)
            return
        senao = self.rotulo("L_else")
        fim = self.rotulo("L_end")
        self.emitir("iffalse", a=cond, opr=senao)
        self.comando(cmd.entao)
        self.emitir("goto", opr=fim)
        self.emitir("label", opr=senao)
        self.comando(cmd.senao)
        self.emitir("label", opr=fim)

    def laco(self, cond: Optional[Node], corpo: Node,
             passo: Optional[Node]) -> None:
        teste = self.rotulo("L_test")
        fim = self.rotulo("L_end")
        ctx = [fim, None if passo is not None else teste]
        self.emitir("label", opr=teste)
        if cond is not None:
            self.emitir("iffalse", a=self.expr(cond), opr=fim)
        self.lacos.append(ctx)
        self.comando(corpo)
        self.lacos.pop()
        if passo is not None:
            if ctx[1] is not None:
                self.emitir("label", opr=ctx[1])
            self.expr(passo)
        self.emitir("goto", opr=teste)
        self.emitir("label", opr=fim)

    def comando_read(self, destino: Node) -> None:
        if isinstance(destino, Id):
            self.emitir("read", dst=self.buscar(destino.nome).nome_ir)
            return
        vetor = self.buscar(destino.alvo.nome).nome_ir
        desl = self.deslocamento(destino.indice)
        t = self.temp()
        self.emitir("read", dst=t)
        self.emitir("store", a=vetor, b=desl, c=t)

    # ---- expressoes ----------------------------------------------------

    def deslocamento(self, indice: Node) -> str:
        i = self.expr(indice)
        t = self.temp()
        self.emitir("bin", dst=t, a=i, opr="*", b="1")
        return t

    def expr(self, no: Node) -> Optional[str]:
        if isinstance(no, Lit):
            return no.valor
        if isinstance(no, Id):
            return self.buscar(no.nome).nome_ir
        if isinstance(no, Assign):
            return self.atribuicao(no)
        if isinstance(no, Binary):
            return self.binaria(no)
        if isinstance(no, Unary):
            a = self.expr(no.operando)
            if no.op == "+":
                return a
            t = self.temp()
            self.emitir("un", dst=t, opr=OPS_UN[no.op], a=a)
            return t
        if isinstance(no, Call):
            return self.chamada(no)
        if isinstance(no, Index):
            vetor = self.buscar(no.alvo.nome).nome_ir
            desl = self.deslocamento(no.indice)
            t = self.temp()
            self.emitir("load", dst=t, a=vetor, b=desl)
            return t
        raise ValueError(no)

    def atribuicao(self, no: Assign) -> str:
        destino = no.destino
        tipo_dest = self.tipo(destino)
        if isinstance(destino, Id):
            nome = self.buscar(destino.nome).nome_ir
            v = self.expr(no.valor)
            v = self.converter(v, self.tipo(no.valor), tipo_dest)
            self.emitir("copy", dst=nome, a=v)
            return nome
        vetor = self.buscar(destino.alvo.nome).nome_ir
        desl = self.deslocamento(destino.indice)
        v = self.expr(no.valor)
        v = self.converter(v, self.tipo(no.valor), tipo_dest)
        self.emitir("store", a=vetor, b=desl, c=v)
        return v

    def binaria(self, no: Binary) -> str:
        if no.op in ("&&", "||") and not simples(no.dir):
            return self.curto_circuito(no)
        a = self.expr(no.esq)
        b = self.expr(no.dir)
        te, td = self.tipo(no.esq), self.tipo(no.dir)
        if "float" in (te, td) and no.op not in ("&&", "||"):
            a = self.converter(a, te, "float")
            b = self.converter(b, td, "float")
        t = self.temp()
        self.emitir("bin", dst=t, a=a, opr=OPS_BIN.get(no.op, no.op), b=b)
        return t

    def curto_circuito(self, no: Binary) -> str:
        a = self.expr(no.esq)
        t = self.temp()
        self.emitir("copy", dst=t, a=a)
        fim = self.rotulo("L_sc")
        self.emitir("iffalse" if no.op == "&&" else "iftrue", a=t, opr=fim)
        b = self.expr(no.dir)
        self.emitir("copy", dst=t, a=b)
        self.emitir("label", opr=fim)
        return t

    def chamada(self, no: Call) -> Optional[str]:
        nome = no.alvo.nome
        tipo_ret, params = self.funcoes[nome]
        args = []
        for arg, (tipo_p, vetor) in zip(no.args, params):
            v = self.expr(arg)
            if not vetor:
                v = self.converter(v, self.tipo(arg), tipo_p)
            args.append(v)
        for v in args:
            self.emitir("param", a=v)
        if tipo_ret == "void":
            self.emitir("call", opr=nome, n=len(args))
            return None
        t = self.temp()
        self.emitir("call", dst=t, opr=nome, n=len(args))
        return t


def simples(no: Node) -> bool:
    # lado direito de && / || sem efeito colateral nem risco de erro em
    # execucao pode ser avaliado sempre, sem curto-circuito
    if isinstance(no, (Lit, Id)):
        return True
    if isinstance(no, Unary):
        return simples(no.operando)
    if isinstance(no, Binary):
        return no.op not in ("/", "%") and simples(no.esq) and simples(no.dir)
    return False


def gerar_ir(programa: Program) -> ProgramaIR:
    return Gerador().gerar(programa)
