from __future__ import annotations

import os
import re
import sys
from typing import Dict, List, Optional, Set, Tuple

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                "..", "ir"))

from minic_ir import INT_LIT, TEMP, Instr, ProgramaIR, Unidade  # noqa: E402

FLOAT_LIT = re.compile(r"-?[0-9]+\.[0-9]+$")
INT_MIN = -2 ** 31
COMUTATIVOS = ("+", "*", "==", "!=", "and", "or")
SALTOS = ("goto", "iffalse", "iftrue")
REMOVIVEIS = ("copy", "bin", "un", "load")


def eh_const(x: Optional[str]) -> bool:
    return x is not None and (x[0].isdigit() or x[0] in "-'\""
                              or x in ("true", "false"))


def eh_bool(x: str) -> bool:
    return x in ("true", "false")


def eh_int(x: str) -> bool:
    return INT_LIT.match(x) is not None


def wrap(v: int) -> int:
    return (v - INT_MIN) % 2 ** 32 + INT_MIN


def texto_bool(v: bool) -> str:
    return "true" if v else "false"


# ---- dobra de constantes e identidades algebricas -----------------------

def dobrar_bin(op: str, a: str, b: str) -> Optional[str]:
    if eh_int(a) and eh_int(b):
        x, y = wrap(int(a)), wrap(int(b))
        if op == "+":
            return str(wrap(x + y))
        if op == "-":
            return str(wrap(x - y))
        if op == "*":
            return str(wrap(x * y))
        if op in ("/", "%"):
            if y == 0 or (x == INT_MIN and y == -1):
                return None
            q = abs(x) // abs(y)
            if (x < 0) != (y < 0):
                q = -q
            return str(q if op == "/" else x - y * q)
        comparacoes = {"<": x < y, "<=": x <= y, ">": x > y, ">=": x >= y,
                       "==": x == y, "!=": x != y}
        if op in comparacoes:
            return texto_bool(comparacoes[op])
        return None
    if eh_bool(a) and eh_bool(b):
        x, y = a == "true", b == "true"
        logicas = {"and": x and y, "or": x or y, "==": x == y, "!=": x != y}
        if op in logicas:
            return texto_bool(logicas[op])
    return None


def identidade(op: str, a: str, b: str) -> Optional[str]:
    if op == "+":
        if a == "0":
            return b
        if b == "0":
            return a
    elif op == "-":
        if b == "0":
            return a
    elif op == "*":
        if a == "1":
            return b
        if b == "1":
            return a
        if a == "0" or b == "0":
            return "0"
    elif op == "/":
        if b == "1":
            return a
    elif op == "and":
        if a == "true":
            return b
        if b == "true":
            return a
        if "false" in (a, b):
            return "false"
    elif op == "or":
        if a == "false":
            return b
        if b == "false":
            return a
        if "true" in (a, b):
            return "true"
    return None


def dobrar_un(op: str, a: str) -> Optional[str]:
    if op == "neg":
        if eh_int(a):
            return str(wrap(-wrap(int(a))))
        if FLOAT_LIT.match(a):
            return a[1:] if a[0] == "-" else "-" + a
    elif op == "not" and eh_bool(a):
        return texto_bool(a != "true")
    elif op == "itof" and eh_int(a):
        return str(wrap(int(a))) + ".0"
    return None


def simplificar(u: Unidade, G: Set[str]) -> bool:
    mudou = False
    novas: List[Instr] = []
    for i in u.instrs:
        r = None
        if i.op == "bin":
            r = dobrar_bin(i.opr, i.a, i.b)
            if r is None:
                r = identidade(i.opr, i.a, i.b)
        elif i.op == "un":
            r = dobrar_un(i.opr, i.a)
        elif i.op in ("iffalse", "iftrue") and eh_bool(i.a):
            mudou = True
            if (i.a == "true") == (i.op == "iftrue"):
                novas.append(Instr("goto", opr=i.opr))
            continue
        if r is not None:
            i = Instr("copy", dst=i.dst, a=r)
            mudou = True
        novas.append(i)
    u.instrs = novas
    return mudou


# ---- blocos basicos e grafo de fluxo ------------------------------------

def usos(i: Instr) -> List[str]:
    op = i.op
    if op in ("copy", "un", "iffalse", "iftrue", "param", "print",
              "return", "array", "global"):
        lista = [i.a]
    elif op == "bin":
        lista = [i.a, i.b]
    elif op == "load":
        lista = [i.b]
    elif op == "store":
        lista = [i.b, i.c]
    else:
        lista = []
    return [x for x in lista if x is not None and not eh_const(x)]


def definida(i: Instr) -> Optional[str]:
    if i.op in ("copy", "bin", "un", "load", "read", "call"):
        return i.dst
    return None


def substituir(i: Instr, troca) -> bool:
    campos = {"copy": "a", "un": "a", "iffalse": "a", "iftrue": "a",
              "param": "a", "print": "a", "return": "a", "array": "a",
              "global": "a", "bin": "ab", "load": "b", "store": "bc"}
    mudou = False
    for campo in campos.get(i.op, ""):
        x = getattr(i, campo)
        if x is None or eh_const(x):
            continue
        y = troca(x)
        if y != x:
            setattr(i, campo, y)
            mudou = True
    return mudou


class Grafo:
    def __init__(self, instrs: List[Instr]):
        n = len(instrs)
        lideres = {0} if n else set()
        for k, i in enumerate(instrs):
            if i.op == "label":
                lideres.add(k)
            elif i.op in SALTOS + ("return",) and k + 1 < n:
                lideres.add(k + 1)
        inicios = sorted(lideres)
        self.blocos: List[Tuple[int, int]] = [
            (ini, inicios[j + 1] if j + 1 < len(inicios) else n)
            for j, ini in enumerate(inicios)]
        nb = len(self.blocos)
        bloco_do_rotulo = {instrs[ini].opr: b
                           for b, (ini, _) in enumerate(self.blocos)
                           if instrs[ini].op == "label"}
        self.suc: List[List[int]] = []
        for b, (_, fim) in enumerate(self.blocos):
            ult = instrs[fim - 1]
            s: List[int] = []
            if ult.op != "goto" and ult.op != "return" and b + 1 < nb:
                s.append(b + 1)
            if ult.op in SALTOS and bloco_do_rotulo[ult.opr] not in s:
                s.append(bloco_do_rotulo[ult.opr])
            self.suc.append(s)
        self.pred: List[List[int]] = [[] for _ in range(nb)]
        for b, s in enumerate(self.suc):
            for x in s:
                self.pred[x].append(b)
        self.alcancaveis: Set[int] = set()
        pilha = [0] if nb else []
        while pilha:
            b = pilha.pop()
            if b not in self.alcancaveis:
                self.alcancaveis.add(b)
                pilha.extend(self.suc[b])


# ---- propagacao de copias e constantes (copias disponiveis, global) ----

def transferir_copias(fatos: Dict[str, str], i: Instr, G: Set[str]) -> None:
    d = definida(i)
    if d is not None:
        for x in [x for x, y in fatos.items() if x == d or y == d]:
            del fatos[x]
    if i.op == "call":
        for x in [x for x, y in fatos.items() if x in G or y in G]:
            del fatos[x]
    if i.op == "copy" and i.dst != i.a:
        fatos[i.dst] = i.a


def propagar(u: Unidade, G: Set[str]) -> bool:
    ins = u.instrs
    g = Grafo(ins)
    nb = len(g.blocos)
    entrada: List[Optional[Dict[str, str]]] = [None] * nb
    saida: List[Optional[Dict[str, str]]] = [None] * nb
    mudou = True
    while mudou:
        mudou = False
        for b in range(nb):
            if b == 0 or not g.pred[b]:
                ent: Optional[Dict[str, str]] = {}
            else:
                ent = None
                for p in g.pred[b]:
                    if saida[p] is None:
                        continue
                    if ent is None:
                        ent = dict(saida[p])
                    else:
                        ent = {x: y for x, y in ent.items()
                               if saida[p].get(x) == y}
            entrada[b] = ent
            if ent is None:
                continue
            fatos = dict(ent)
            ini, fim = g.blocos[b]
            for i in ins[ini:fim]:
                transferir_copias(fatos, i, G)
            if fatos != saida[b]:
                saida[b] = fatos
                mudou = True

    alterou = False
    for b, (ini, fim) in enumerate(g.blocos):
        fatos = dict(entrada[b] or {})
        for i in ins[ini:fim]:
            if substituir(i, lambda x: fatos.get(x, x)):
                alterou = True
            transferir_copias(fatos, i, G)
    return alterou


# ---- subexpressoes comuns (local, por bloco) ----------------------------

def chave(i: Instr) -> Tuple[str, str, str, str]:
    a, b = i.a, i.b if i.op == "bin" else ""
    if i.op == "bin" and i.opr in COMUTATIVOS and b < a:
        a, b = b, a
    return (i.op, i.opr, a, b)


def subexpressoes(u: Unidade, G: Set[str]) -> bool:
    ins = u.instrs
    mudou = False
    for ini, fim in Grafo(ins).blocos:
        tabela: Dict[Tuple[str, str, str, str], str] = {}
        for k in range(ini, fim):
            i = ins[k]
            if i.op in ("bin", "un"):
                dono = tabela.get(chave(i))
                if dono is not None and dono != i.dst:
                    i = ins[k] = Instr("copy", dst=i.dst, a=dono)
                    mudou = True
            d = definida(i)
            if d is not None:
                tabela = {c: v for c, v in tabela.items()
                          if v != d and d not in c[2:]}
            if i.op == "call":
                tabela = {c: v for c, v in tabela.items()
                          if v not in G and not (set(c[2:]) & G)}
            if i.op in ("bin", "un") and i.dst not in chave(i)[2:]:
                tabela[chave(i)] = i.dst
    return mudou


# ---- codigo morto (variaveis vivas, global) -----------------------------

def usos_vivos(i: Instr, G: Set[str]) -> Set[str]:
    s = set(usos(i))
    if i.op == "call":
        s |= G
    return s


def codigo_morto(u: Unidade, G: Set[str]) -> bool:
    ins = u.instrs
    g = Grafo(ins)
    nb = len(g.blocos)
    vivas_ent: List[Set[str]] = [set() for _ in range(nb)]
    vivas_sai: List[Set[str]] = [set() for _ in range(nb)]
    mudou = True
    while mudou:
        mudou = False
        for b in reversed(range(nb)):
            sai = set(G) if not g.suc[b] else set()
            for s in g.suc[b]:
                sai |= vivas_ent[s]
            vivas = set(sai)
            ini, fim = g.blocos[b]
            for i in reversed(ins[ini:fim]):
                d = definida(i)
                if d is not None:
                    vivas.discard(d)
                vivas |= usos_vivos(i, G)
            if sai != vivas_sai[b] or vivas != vivas_ent[b]:
                vivas_sai[b], vivas_ent[b] = sai, vivas
                mudou = True

    alterou = False
    remover: Set[int] = set()
    for b, (ini, fim) in enumerate(g.blocos):
        vivas = set(vivas_sai[b])
        for k in reversed(range(ini, fim)):
            i = ins[k]
            d = definida(i)
            if i.op == "copy" and i.dst == i.a:
                remover.add(k)
                continue
            if i.op in REMOVIVEIS and d not in vivas:
                remover.add(k)
                continue
            if i.op == "call" and d is not None and d not in vivas:
                i.dst = None
                d = None
                alterou = True
            if d is not None:
                vivas.discard(d)
            vivas |= usos_vivos(i, G)
    if remover:
        u.instrs = [i for k, i in enumerate(ins) if k not in remover]
        alterou = True
    return alterou


def contar(ins: List[Instr]) -> Tuple[Dict[str, int], Dict[str, int]]:
    nusos: Dict[str, int] = {}
    ndefs: Dict[str, int] = {}
    for i in ins:
        for x in usos(i):
            nusos[x] = nusos.get(x, 0) + 1
        d = definida(i)
        if d is not None:
            ndefs[d] = ndefs.get(d, 0) + 1
    return nusos, ndefs


def juntar_copias(u: Unidade, G: Set[str]) -> bool:
    # t = a + b; x = t   ->   x = a + b   (t temporario usado so ali)
    ins = u.instrs
    nusos, ndefs = contar(ins)
    mudou = False
    k = 0
    while k + 1 < len(ins):
        i, j = ins[k], ins[k + 1]
        t = definida(i)
        if (i.op in ("copy", "bin", "un", "load", "call") and t is not None
                and TEMP.match(t) and j.op == "copy" and j.a == t
                and j.dst != t and nusos.get(t) == 1 and ndefs.get(t) == 1):
            i.dst = j.dst
            del ins[k + 1]
            mudou = True
        k += 1
    return mudou


# ---- fluxo de controle: inalcancavel, saltos e rotulos ------------------

def limpar_fluxo(u: Unidade, G: Set[str]) -> bool:
    mudou = False
    g = Grafo(u.instrs)
    if len(g.alcancaveis) < len(g.blocos):
        u.instrs = [i for b, (ini, fim) in enumerate(g.blocos)
                    if b in g.alcancaveis for i in u.instrs[ini:fim]]
        mudou = True

    ins = u.instrs
    novas: List[Instr] = []
    for k, i in enumerate(ins):
        if i.op in SALTOS:
            j = k + 1
            inutil = False
            while j < len(ins) and ins[j].op == "label":
                if ins[j].opr == i.opr:
                    inutil = True
                    break
                j += 1
            if inutil:
                mudou = True
                continue
        novas.append(i)

    alvos = {i.opr for i in novas if i.op in SALTOS}
    u.instrs = [i for i in novas if i.op != "label" or i.opr in alvos]
    return mudou or len(u.instrs) != len(novas)


# ---- movimento de codigo invariante em lacos ----------------------------

def dominadores(g: Grafo) -> List[Set[int]]:
    nb = len(g.blocos)
    todos = set(g.alcancaveis)
    dom = [set(todos) for _ in range(nb)]
    if nb:
        dom[0] = {0}
    mudou = True
    while mudou:
        mudou = False
        for b in range(1, nb):
            if b not in todos:
                continue
            novo = set(todos)
            for p in g.pred[b]:
                if p in todos:
                    novo &= dom[p]
            novo.add(b)
            if novo != dom[b]:
                dom[b] = novo
                mudou = True
    return dom


def mover_invariantes(u: Unidade, G: Set[str]) -> bool:
    ins = u.instrs
    g = Grafo(ins)
    dom = dominadores(g)
    lacos: Dict[int, Set[int]] = {}
    for b in sorted(g.alcancaveis):
        for h in g.suc[b]:
            if h in dom[b]:
                corpo = lacos.setdefault(h, {h})
                pilha = [b]
                while pilha:
                    m = pilha.pop()
                    if m not in corpo:
                        corpo.add(m)
                        pilha.extend(p for p in g.pred[m]
                                     if p in g.alcancaveis)

    nusos, ndefs = contar(ins)
    for h in sorted(lacos):
        corpo = lacos[h]
        if h == 0:
            continue
        fora = [p for p in g.pred[h] if p not in corpo and p in g.alcancaveis]
        if fora != [h - 1]:
            continue
        if ins[g.blocos[h - 1][1] - 1].op in SALTOS + ("return",):
            continue

        indices = [k for b in sorted(corpo) for k in range(*g.blocos[b])]
        no_laco = set(indices)
        defs_laco = {definida(ins[k]) for k in indices} - {None}
        tem_chamada = any(ins[k].op == "call" for k in indices)
        usos_fora = set()
        for k, i in enumerate(ins):
            if k not in no_laco:
                usos_fora |= set(usos(i))

        movidas: List[int] = []
        movidos: Set[str] = set()
        progresso = True
        while progresso:
            progresso = False
            for k in indices:
                i = ins[k]
                if k in movidas or i.op not in ("bin", "un"):
                    continue
                if i.op == "bin" and i.opr in ("/", "%"):
                    continue
                d = i.dst
                if not TEMP.match(d) or ndefs.get(d) != 1 or d in usos_fora:
                    continue
                operandos = [i.a, i.b] if i.op == "bin" else [i.a]
                if all(eh_const(x) or x in movidos
                       or (x not in defs_laco and not (tem_chamada and x in G))
                       for x in operandos):
                    movidas.append(k)
                    movidos.add(d)
                    progresso = True
        if movidas:
            antes = g.blocos[h][0]
            novas: List[Instr] = []
            for k, i in enumerate(ins):
                if k == antes:
                    novas.extend(ins[m] for m in movidas)
                if k not in movidas:
                    novas.append(i)
            u.instrs = novas
            return True
    return False


PASSOS = (simplificar, propagar, subexpressoes, codigo_morto, juntar_copias,
          limpar_fluxo, mover_invariantes)


def otimizar_unidade(u: Unidade, G: Set[str]) -> None:
    for _ in range(100):
        mudou = False
        for passo in PASSOS:
            if passo(u, G):
                mudou = True
        if not mudou:
            break


def otimizar(prog: ProgramaIR) -> ProgramaIR:
    G = set(prog.globais)
    otimizar_unidade(prog.init, G)
    for f in prog.funcoes:
        otimizar_unidade(f, G)
    return prog
