from __future__ import annotations

import re
import sys
from typing import Dict, List, Optional, Tuple

INT_MIN = -(2**31)
LIMITE_PASSOS = 5_000_000

ESCAPES = {"n": "\n", "t": "\t", "\\": "\\", "'": "'", '"': '"'}


class ErroExecucao(Exception):
    pass


def wrap(v: int) -> int:
    return (v - INT_MIN) % 2**32 + INT_MIN


class Vetor:
    def __init__(self, tamanho: Optional[int]):
        self.tamanho = tamanho
        self.dados: Dict[int, object] = {}

    def checar(self, i) -> int:
        if not isinstance(i, int) or isinstance(i, bool):
            raise ErroExecucao(f"indice invalido {i!r}")
        if self.tamanho is not None and not 0 <= i < self.tamanho:
            raise ErroExecucao(f"indice {i} fora do vetor de {self.tamanho}")
        return i


def literal(tok: str):
    if tok == "true":
        return True
    if tok == "false":
        return False
    if tok.startswith('"'):
        corpo = tok[1:-1]
        return re.sub(r"\\(.)", lambda m: ESCAPES.get(m.group(1), m.group(1)), corpo)
    if tok.startswith("'"):
        corpo = tok[1:-1]
        if corpo.startswith("\\"):
            corpo = ESCAPES.get(corpo[1], corpo[1])
        return ord(corpo)
    if re.fullmatch(r"-?[0-9]+", tok):
        return wrap(int(tok))
    if re.fullmatch(r"-?[0-9]+\.[0-9]+", tok):
        return float(tok)
    return None


RE_LINHAS = [
    ("label", re.compile(r"(\S+):$")),
    ("goto", re.compile(r"goto (\S+)$")),
    ("iffalse", re.compile(r"ifFalse (\S+) goto (\S+)$")),
    ("iftrue", re.compile(r"ifTrue (\S+) goto (\S+)$")),
    ("param", re.compile(r"param(?: (.+))?$")),
    ("call", re.compile(r"(?:(\S+) = )?call ([^,\s]+), *(\d+)$")),
    ("return", re.compile(r"return(?: (.+))?$")),
    ("print", re.compile(r"print (.+)$")),
    ("read", re.compile(r"read (\S+)$")),
    ("store", re.compile(r"store ([^\[\s]+)\[(\S+)\], (\S+)$")),
    ("load", re.compile(r"(\S+) = load ([^\[\s]+)\[(\S+)\]$")),
    ("array", re.compile(r"array ([^\[\s]+)\[(\S+)\]$")),
    ("global", re.compile(r"global ([^\[\s]+)(?:\[(\S+)\])?$")),
    ("un", re.compile(r"(\S+) = (not|neg|itof) (\S+)$")),
    ("bin", re.compile(r"(\S+) = (\S+) (\S+) (\S+)$")),
    ("copy", re.compile(r"(\S+) = (\S+)$")),
]


def analisar_linha(linha: str) -> Tuple:
    for op, rx in RE_LINHAS:
        m = rx.match(linha)
        if m:
            return (op,) + m.groups()
    raise ErroExecucao(f"linha de IR nao reconhecida: {linha!r}")


class Funcao:
    def __init__(self, nome: str, params: List[str]):
        self.nome = nome
        self.params = params
        self.codigo: List[Tuple] = []
        self.rotulos: Dict[str, int] = {}


def carregar(texto: str) -> Tuple[Optional[Funcao], Dict[str, Funcao]]:
    funcoes: Dict[str, Funcao] = {}
    init: Optional[Funcao] = None
    atual: Optional[Funcao] = None
    for bruta in texto.splitlines():
        linha = bruta.strip()
        if not linha:
            continue
        if linha == "init":
            atual = init = Funcao("init", [])
            continue
        m = re.match(r"func (\w+)(?:\((.*)\))?$", linha)
        if m:
            params = [
                p.strip().replace("[]", "")
                for p in (m.group(2) or "").split(",")
                if p.strip()
            ]
            atual = funcoes[m.group(1)] = Funcao(m.group(1), params)
            continue
        if linha in ("endfunc", "endinit"):
            atual = None
            continue
        if atual is None:
            raise ErroExecucao(f"instrucao fora de funcao: {linha!r}")
        instr = analisar_linha(linha)
        if instr[0] == "label":
            atual.rotulos[instr[1]] = len(atual.codigo)
        atual.codigo.append(instr)
    return init, funcoes


def formatar(v) -> str:
    if isinstance(v, bool):
        return "true" if v else "false"
    if isinstance(v, float):
        return repr(v)
    return str(v)


class Maquina:
    def __init__(self, texto: str, entrada: str = ""):
        self.init, self.funcoes = carregar(texto)
        self.globais: Dict[str, object] = {}
        self.saida: List[str] = []
        self.entrada = entrada.split()
        self.passos = 0

    def valor(self, tok: str, locais: Dict[str, object]):
        lit = literal(tok)
        if lit is not None:
            return lit
        if tok in locais:
            return locais[tok]
        if tok in self.globais:
            return self.globais[tok]
        return 0

    def gravar(self, nome: str, v, locais: Dict[str, object]) -> None:
        if nome in self.globais and nome not in locais:
            self.globais[nome] = v
        else:
            locais[nome] = v

    def vetor(self, nome: str, locais: Dict[str, object]) -> Vetor:
        v = locais.get(nome, self.globais.get(nome))
        if not isinstance(v, Vetor):
            v = Vetor(None)
            self.gravar(nome, v, locais)
        return v

    def executar(self):
        if self.init is not None:
            self.rodar(self.init, [], global_=True)
        if "main" not in self.funcoes:
            raise ErroExecucao("programa sem main")
        return self.rodar(self.funcoes["main"], [])

    def rodar(self, f: Funcao, args: List[object], global_: bool = False):
        locais: Dict[str, object] = dict(zip(f.params, args))
        pendentes: List[object] = []
        pc = 0
        while pc < len(f.codigo):
            self.passos += 1
            if self.passos > LIMITE_PASSOS:
                raise ErroExecucao("limite de passos excedido")
            instr = f.codigo[pc]
            op = instr[0]
            pc += 1
            if op == "label":
                continue
            if op == "goto":
                pc = f.rotulos[instr[1]]
            elif op in ("iffalse", "iftrue"):
                c = self.valor(instr[1], locais)
                if bool(c) == (op == "iftrue"):
                    pc = f.rotulos[instr[2]]
            elif op == "param":
                if instr[1] is not None:
                    pendentes.append(self.valor(instr[1], locais))
            elif op == "call":
                n = int(instr[3])
                chamados = pendentes[len(pendentes) - n :] if n else []
                del pendentes[len(pendentes) - n :]
                if instr[2] not in self.funcoes:
                    raise ErroExecucao(f"funcao {instr[2]} nao existe")
                r = self.rodar(self.funcoes[instr[2]], chamados)
                if instr[1]:
                    self.gravar(instr[1], r, locais)
            elif op == "return":
                return self.valor(instr[1], locais) if instr[1] is not None else None
            elif op == "print":
                self.saida.append(formatar(self.valor(instr[1], locais)))
            elif op == "read":
                if not self.entrada:
                    raise ErroExecucao("entrada acabou")
                tok = self.entrada.pop(0)
                self.gravar(instr[1], literal(tok), locais)
            elif op == "store":
                vet = self.vetor(instr[1], locais)
                vet.dados[vet.checar(self.valor(instr[2], locais))] = self.valor(
                    instr[3], locais
                )
            elif op == "load":
                vet = self.vetor(instr[2], locais)
                i = vet.checar(self.valor(instr[3], locais))
                self.gravar(instr[1], vet.dados.get(i, 0), locais)
            elif op in ("array", "global"):
                tamanho = instr[2]
                if tamanho is None:
                    self.globais.setdefault(instr[1], 0)
                    continue
                vet = Vetor(self.valor(tamanho, locais))
                if op == "global":
                    self.globais[instr[1]] = vet
                else:
                    locais[instr[1]] = vet
            elif op == "un":
                a = self.valor(instr[3], locais)
                if instr[2] == "not":
                    r = not a
                elif instr[2] == "itof":
                    r = float(a)
                else:
                    r = -a if isinstance(a, float) else wrap(-a)
                self.gravar(instr[1], r, locais)
            elif op == "bin":
                a = self.valor(instr[2], locais)
                b = self.valor(instr[4], locais)
                self.gravar(instr[1], binaria(instr[3], a, b), locais)
            elif op == "copy":
                self.gravar(instr[1], self.valor(instr[2], locais), locais)
        return None


def binaria(op: str, a, b):
    if op == "and":
        return bool(a) and bool(b)
    if op == "or":
        return bool(a) or bool(b)
    if op in ("==", "!="):
        igual = a == b
        return igual if op == "==" else not igual
    if op in ("<", "<=", ">", ">="):
        return {"<": a < b, "<=": a <= b, ">": a > b, ">=": a >= b}[op]
    real = isinstance(a, float) or isinstance(b, float)
    if op == "+":
        r = a + b
    elif op == "-":
        r = a - b
    elif op == "*":
        r = a * b
    elif op in ("/", "%"):
        if b == 0:
            raise ErroExecucao("divisao por zero")
        if real:
            r = a / b if op == "/" else a % b
        else:
            q = abs(a) // abs(b)
            if (a < 0) != (b < 0):
                q = -q
            r = q if op == "/" else a - b * q
    else:
        raise ErroExecucao(f"operador desconhecido {op}")
    return r if real else wrap(r)


def executar(texto: str, entrada: str = ""):
    """Devolve (linhas impressas, retorno de main, erro ou None)."""
    m = Maquina(texto, entrada)
    try:
        r = m.executar()
        return m.saida, r, None
    except (ErroExecucao, RecursionError) as e:
        return m.saida, None, str(e)


def main(argv: List[str]) -> int:
    if len(argv) < 2:
        print("uso: ir_interp.py programa.ir [entrada]", file=sys.stderr)
        return 1
    with open(argv[1], encoding="utf-8") as f:
        texto = f.read()
    entrada = ""
    if len(argv) > 2:
        with open(argv[2], encoding="utf-8") as f:
            entrada = f.read()
    saida, retorno, erro = executar(texto, entrada)
    for linha in saida:
        print(linha)
    if erro:
        print(f"erro de execucao: {erro}", file=sys.stderr)
        return 6
    return retorno if isinstance(retorno, int) else 0


if __name__ == "__main__":
    sys.setrecursionlimit(20000)
    sys.exit(main(sys.argv))
