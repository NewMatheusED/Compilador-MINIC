from __future__ import annotations

import os
import re
import subprocess
import sys
import tempfile
from typing import Dict, List, Optional, Tuple

AQUI = os.path.dirname(os.path.abspath(__file__))
RAIZ = os.path.dirname(os.path.dirname(AQUI))
sys.path.insert(0, AQUI)

from ir_interp import executar  # noqa: E402

SUITE = os.path.join(AQUI, "minic-ir-test-cases", "testes")
EXTRAS = os.path.join(AQUI, "extras")


def ler(caminho: str) -> str:
    with open(caminho, encoding="utf-8") as f:
        return f.read()


class Compilador:
    def __init__(self, usar_c: bool):
        self.tmp = tempfile.mkdtemp()
        if usar_c:
            exe = os.path.join(self.tmp, "minic")
            subprocess.run(
                [
                    "gcc",
                    "-Wall",
                    "-Wextra",
                    "-std=c11",
                    "-O2",
                    os.path.join(RAIZ, "minic.c"),
                    "-o",
                    exe,
                ],
                check=True,
            )
            self.cmd = [exe]
            self.nome = "minic.c"
        else:
            self.cmd = [sys.executable, os.path.join(RAIZ, "minic.py")]
            self.nome = "minic.py"

    def rodar(self, *args: str) -> Tuple[int, str, str]:
        p = subprocess.run(self.cmd + list(args), capture_output=True)
        return (p.returncode, p.stdout.decode("utf-8"), p.stderr.decode("utf-8"))


# ---- leitura dos gabaritos --------------------------------------------

CABECALHOS = (
    "Resultado",
    "Modo",
    "Alvo",
    "Verificar",
    "Observação",
    "Critério",
    "Categoria",
    "IR ",
)


def secao(gabarito: str, inicio: str) -> List[str]:
    linhas = gabarito.splitlines()
    for k, linha in enumerate(linhas):
        if linha.startswith(inicio):
            corpo = []
            for prox in linhas[k + 1 :]:
                if prox.startswith(CABECALHOS):
                    break
                if prox.strip():
                    corpo.append(prox.rstrip())
            return corpo
    return []


def instrucoes(linhas: List[str]) -> List[str]:
    return [
        l.strip()
        for l in linhas
        if l.strip() and not re.match(r"(func\b|endfunc|init|endinit)", l.strip())
    ]


# ---- normalizacao -----------------------------------------------------


def normalizar(texto: str) -> List[str]:
    saida: List[str] = []
    for funcao in re.split(r"(?m)^(?=func |init$)", texto):
        linhas = [l.strip() for l in funcao.splitlines() if l.strip()]
        temps: Dict[str, str] = {}
        rotulos: Dict[str, str] = {}
        variantes: Dict[str, List[str]] = {}
        tokens = re.findall(r"[A-Za-z_][\w#]*", " ".join(linhas))
        defs = {l[:-1] for l in linhas if l.endswith(":")}
        for t in tokens:
            if t in defs:
                rotulos.setdefault(t, f"L{len(rotulos) + 1}")
            elif re.fullmatch(r"t\d+", t):
                temps.setdefault(t, f"T{len(temps) + 1}")
            else:
                base = t.split("#")[0]
                lista = variantes.setdefault(base, [])
                if t not in lista:
                    lista.append(t)

        def troca(m: re.Match) -> str:
            t = m.group(0)
            if t in rotulos:
                return rotulos[t]
            if t in temps:
                return temps[t]
            base = t.split("#")[0]
            lista = variantes.get(base, [t])
            if len(lista) > 1:
                return f"{base}~{lista.index(t) + 1}"
            return t

        saida += [re.sub(r"[A-Za-z_][\w#]*", troca, l) for l in linhas]
    return saida


class Saida(list):
    def __str__(self) -> str:
        if len(self) > 8:
            return str(list(self[:8]))[:-1] + ", ...]"
        return str(list(self))

    __repr__ = __str__


def comportamento(ir: str, entrada: str = ""):
    saida, retorno, erro = executar(ir, entrada)
    return Saida(saida), erro


# ---- casos ------------------------------------------------------------


class Placar:
    def __init__(self):
        self.total: Dict[str, int] = {}
        self.ok: Dict[str, int] = {}

    def marcar(self, grupo: str, nome: str, passou: bool, detalhe: str):
        self.total[grupo] = self.total.get(grupo, 0) + 1
        if passou:
            self.ok[grupo] = self.ok.get(grupo, 0) + 1
        estado = "OK" if passou else "FALHA"
        print(f"[{estado}][{grupo}] {nome} — {detalhe}")


def caso_valido(c: Compilador, fonte: str, gabarito: str, p: Placar):
    nome = os.path.basename(fonte)
    rc0, ir0, err0 = c.rodar("--ir", "-O0", fonte)
    rc1, ir1, err1 = c.rodar("--ir", "-O1", fonte)
    if rc0 != 0 or rc1 != 0:
        p.marcar(
            "normais",
            nome,
            False,
            f"codigo de saida {rc0}/{rc1}: {(err0 or ir0).strip()}",
        )
        return
    referencia = "\n".join(secao(gabarito, "IR canônica esperada")) + "\n"
    s0, e0 = comportamento(ir0)
    s1, e1 = comportamento(ir1)
    sr, er = comportamento(referencia)
    if e0 or e1 or s0 != s1:
        p.marcar(
            "normais", nome, False, f"-O0 imprime {s0} ({e0}), -O1 imprime {s1} ({e1})"
        )
        return
    if normalizar(ir0) == normalizar(referencia):
        p.marcar(
            "normais", nome, True, f"IR igual ao gabarito (normalizada); saída {s0}"
        )
    elif not er and sr == s0:
        p.marcar("normais", nome, True, f"IR equivalente ao gabarito: mesma saída {s0}")
    else:
        p.marcar("normais", nome, False, f"saída {s0}, referência {sr} ({er})")


def caso_erro(c: Compilador, fonte: str, gabarito: str, p: Placar):
    nome = os.path.basename(fonte)
    m = re.search(r"código de saída (\d+)", gabarito)
    esperado = int(m.group(1)) if m else None
    rc, out, err = c.rodar("--ir", fonte)
    diag = (err or out).strip().splitlines()
    diag = diag[0] if diag else ""
    emitiu_ir = re.search(r"(?m)^func ", out) is not None
    passou = rc == esperado and not emitiu_ir
    detalhe = f"saída {rc} (esperado {esperado})"
    if emitiu_ir:
        detalhe += ", mas emitiu IR"
    p.marcar("erros", nome, passou, f"{detalhe}: {diag}")


def caso_otimizacao(c: Compilador, fonte: str, gabarito: str, p: Placar):
    nome = os.path.basename(fonte)
    rc0, ir0, _ = c.rodar("--ir", "-O0", fonte)
    rc1, ir1, err = c.rodar("--ir", "-O1", fonte)
    if rc0 != 0 or rc1 != 0:
        p.marcar("otimizacao", nome, False, f"codigo de saida {rc0}/{rc1}")
        return
    s0, e0 = comportamento(ir0)
    s1, e1 = comportamento(ir1)
    ref = instrucoes(secao(gabarito, "IR otimizada"))
    n0 = len(instrucoes(ir0.splitlines()))
    n1 = len(instrucoes(ir1.splitlines()))
    if e0 or e1 or s0 != s1:
        p.marcar(
            "otimizacao",
            nome,
            False,
            f"-O0 imprime {s0} ({e0}), -O1 imprime {s1} ({e1})",
        )
    elif n1 > len(ref):
        p.marcar(
            "otimizacao",
            nome,
            False,
            f"-O1 com {n1} instruções, referência com {len(ref)}",
        )
    else:
        p.marcar(
            "otimizacao",
            nome,
            True,
            f"{n0} -> {n1} instruções (referência {len(ref)}); "
            f"saída {s1} igual nas duas",
        )


def caso_extra(c: Compilador, fonte: str, p: Placar):
    nome = os.path.basename(fonte)
    base = fonte[:-2]
    entrada = ler(base + ".entrada") if os.path.exists(base + ".entrada") else ""
    esperado = ler(base + ".saida").split("\n")[:-1]
    resultados = []
    for nivel in ("-O0", "-O1"):
        rc, ir, err = c.rodar("--ir", nivel, fonte)
        if rc != 0:
            p.marcar("extras", nome, False, f"{nivel}: saída {rc} {err.strip()}")
            return
        saida, erro = comportamento(ir, entrada)
        resultados.append((saida, erro, len(instrucoes(ir.splitlines()))))
    (s0, e0, n0), (s1, e1, n1) = resultados
    passou = s0 == esperado and s1 == esperado and not e0 and not e1
    detalhe = (
        f"{n0} -> {n1} instruções; saída {s1}"
        if passou
        else f"esperado {esperado}, -O0 {s0} ({e0}), -O1 {s1} ({e1})"
    )
    p.marcar("extras", nome, passou, detalhe)


def paridade() -> int:
    py, c = Compilador(False), Compilador(True)
    fontes = []
    for pasta in (os.path.join(AQUI), os.path.join(RAIZ, "tests", "etapa3")):
        for raiz, _, arquivos in os.walk(pasta):
            fontes += [os.path.join(raiz, a) for a in arquivos if a.endswith(".c")]
    iguais = diferentes = 0
    for fonte in sorted(fontes):
        for nivel in ("-O0", "-O1"):
            if py.rodar("--ir", nivel, fonte) == c.rodar("--ir", nivel, fonte):
                iguais += 1
            else:
                diferentes += 1
                print(f"[DIFERENTE] {os.path.relpath(fonte, RAIZ)} {nivel}")
    print(
        f"paridade C x Python: {iguais} iguais, {diferentes} diferentes "
        f"({len(fontes)} programas, -O0 e -O1)"
    )
    return 0 if diferentes == 0 else 1


def main(argv: List[str]) -> int:
    if "--paridade" in argv:
        return paridade()
    c = Compilador("--c" in argv)
    print(f"compilador: {c.nome}\n")
    p = Placar()
    for pasta, func in (
        ("01-validos-sem-otimizacao", caso_valido),
        ("02-erros", caso_erro),
        ("03-otimizacoes", caso_otimizacao),
    ):
        raiz = os.path.join(SUITE, pasta)
        for caso in sorted(os.listdir(raiz)):
            fonte = os.path.join(raiz, caso, caso + ".c")
            func(c, fonte, ler(os.path.join(raiz, caso, caso + ".txt")), p)
    for arq in sorted(os.listdir(EXTRAS)):
        if arq.endswith(".c"):
            caso_extra(c, os.path.join(EXTRAS, arq), p)

    print("\nResumo:")
    nomes = {
        "normais": "Testes normais",
        "erros": "Testes com erro",
        "otimizacao": "Testes otimização",
        "extras": "Extras do grupo",
    }
    for g, rotulo in nomes.items():
        print(f"  {rotulo + ':':21}{p.ok.get(g, 0)}/{p.total.get(g, 0)} aprovados")
    return 0 if p.ok == p.total else 1


if __name__ == "__main__":
    sys.setrecursionlimit(20000)
    sys.exit(main(sys.argv))
