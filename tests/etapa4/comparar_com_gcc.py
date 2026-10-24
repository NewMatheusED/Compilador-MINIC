import os
import random
import subprocess
import sys
import tempfile

AQUI = os.path.dirname(os.path.abspath(__file__))
RAIZ = os.path.dirname(os.path.dirname(AQUI))
sys.path.insert(0, AQUI)

from ir_interp import executar  # noqa: E402

PRELUDIO = (
    "#include <stdio.h>\n#include <stdbool.h>\n"
    '#define print(x) printf("%d\\n", (int)(x))\n'
)


class Gen:
    def __init__(self, rnd):
        self.r = rnd
        self.n = 0

    def nome(self, p="v"):
        self.n += 1
        return f"{p}{self.n}"

    def iexpr(self, vs, d=0):
        r = self.r
        if d > 2 or r.random() < 0.3:
            if vs and r.random() < 0.7:
                return r.choice(vs)
            return str(r.randint(0, 9))
        k = r.randint(0, 9)
        a = self.iexpr(vs, d + 1)
        b = self.iexpr(vs, d + 1)
        if k <= 1:
            return f"({a} + {b})"
        if k == 2:
            return f"({a} - {b})"
        if k == 3:
            return f"({a} * {b})"
        if k == 4:
            return f"({a} / ({b} % 5 + 6))"
        if k == 5:
            return f"({a} % ({b} % 5 + 6))"
        if k == 6:
            return f"(-{a})"
        if k == 7:
            return f"({a} * 1)"
        if k == 8:
            return f"ga[(({a}) % 4 + 4) % 4]"
        return f"({a} + 0)"

    def cond(self, vs, d=0):
        r = self.r
        k = r.randint(0, 6)
        a, b = self.iexpr(vs, 1), self.iexpr(vs, 1)
        if d < 2 and k == 0:
            return f"({self.cond(vs, d + 1)} && {self.cond(vs, d + 1)})"
        if d < 2 and k == 1:
            return f"({self.cond(vs, d + 1)} || {self.cond(vs, d + 1)})"
        if d < 2 and k == 2:
            return f"!{self.cond(vs, d + 1)}"
        if k == 3:
            return f"(espia({a}) > {b})"
        return f"({a} {r.choice(['<', '<=', '>', '>=', '==', '!='])} {b})"

    def stmts(self, vs, prof, laco, n):
        out = []
        for _ in range(n):
            out.append(self.stmt(vs, prof, laco))
        return "\n".join(out)

    def stmt(self, vs, prof, laco):
        r = self.r
        k = r.randint(0, 12)
        mut = [v for v in vs if not v.startswith("c")]
        if k <= 2 and mut:
            return f"{r.choice(mut)} = {self.iexpr(vs)};"
        if k == 3:
            return f"print({self.iexpr(vs)});"
        if k == 4 and prof < 3:
            s = f"if ({self.cond(vs)}) {{\n{self.stmts(vs, prof + 1, laco, 2)}\n}}"
            if r.random() < 0.5:
                s += f" else {{\n{self.stmts(vs, prof + 1, laco, 2)}\n}}"
            return s
        if k == 5 and prof < 3:
            c = self.nome("c")
            corpo = self.stmts(vs + [c], prof + 1, True, 3)
            if r.random() < 0.5:
                return (
                    f"{{ int {c} = 0;\nwhile ({c} < {r.randint(1, 4)}) {{\n"
                    f"{c} = {c} + 1;\n{corpo}\n}} }}"
                )
            return (
                f"{{ int {c};\nfor ({c} = 0; {c} < {r.randint(1, 4)}; "
                f"{c} = {c} + 1) {{\n{corpo}\n}} }}"
            )
        if k == 6 and laco:
            return f"if ({self.cond(vs)}) {r.choice(['break', 'continue'])};"
        if k == 7 and prof < 3:
            v = r.choice(vs) if vs and r.random() < 0.5 else self.nome()
            v = v if not v.startswith("c") else self.nome()
            return (
                f"{{ int {v} = {self.iexpr([x for x in vs if x != v])};\n"
                f"{self.stmts(vs + [v], prof + 1, laco, 2)}\n}}"
            )
        if k == 8:
            return f"ga[(({self.iexpr(vs)}) % 4 + 4) % 4] = {self.iexpr(vs)};"
        if k == 9:
            return f"mexe({self.iexpr(vs)});"
        if k == 11 and self.funcs and mut:
            f = r.choice(self.funcs)
            return f"{r.choice(mut)} = {f}({self.iexpr(vs)}, {self.iexpr(vs)});"
        if k == 10:
            return f"g = g + {self.iexpr(vs)};"
        return f"print({self.iexpr(vs)});"

    def programa(self):
        self.funcs = []
        partes = [
            "int g = 3;",
            "int ga[4];",
            "void mexe(int a) {\n g = g + a;\n ga[1] = a;\n}",
            "int espia(int a) {\n print(a);\n return a;\n}",
        ]
        self.funcs = []
        for _ in range(self.r.randint(0, 2)):
            f = self.nome("f")
            corpo = self.stmts(["a", "b", "g"], 1, False, 3)
            partes.append(
                f"int {f}(int a, int b) {{\n{corpo}\n"
                f"return {self.iexpr(['a', 'b', 'g'])};\n}}"
            )
            self.funcs.append(f)
        vs = []
        decls = []
        for _ in range(3):
            v = self.nome()
            decls.append(f"int {v} = {self.iexpr(vs)};")
            vs.append(v)
        corpo = self.stmts(vs + ["g"], 1, False, 8)
        partes.append(
            "int main() {\n"
            + "\n".join(decls)
            + "\n"
            + corpo
            + "\nprint(g);\nprint(ga[0] + ga[1] + ga[2] + ga[3]);"
            "\nreturn 0;\n}"
        )
        return "\n".join(partes) + "\n"


def rodar(cmd):
    return subprocess.run(cmd, capture_output=True, text=True, timeout=10)


def main(argv):
    semente = int(argv[1]) if len(argv) > 1 else 1
    quantidade = int(argv[2]) if len(argv) > 2 else 100
    tmp = tempfile.mkdtemp()
    minic_c = os.path.join(tmp, "minic")
    subprocess.run(
        ["gcc", "-O2", "-std=c11", os.path.join(RAIZ, "minic.c"), "-o", minic_c],
        check=True,
    )
    minic_py = [sys.executable, os.path.join(RAIZ, "minic.py")]
    fonte = os.path.join(tmp, "p.c")
    como_c = os.path.join(tmp, "p_gcc.c")
    exe = os.path.join(tmp, "p_gcc")
    falhas = 0
    for k in range(quantidade):
        programa = Gen(random.Random(semente * 100000 + k)).programa()
        with open(fonte, "w") as f:
            f.write(programa)
        with open(como_c, "w") as f:
            f.write(PRELUDIO + programa)
        rodar(["gcc", "-w", "-fwrapv", como_c, "-o", exe])
        try:
            esperado = rodar([exe]).stdout.split()
        except subprocess.TimeoutExpired:
            continue
        problema = None
        for nivel in ("-O0", "-O1"):
            py = rodar(minic_py + ["--ir", nivel, fonte])
            c = rodar([minic_c, "--ir", nivel, fonte])
            if py.returncode or py.stdout != c.stdout:
                problema = f"{nivel}: C e Python diferentes ou erro"
                break
            saida, _, erro = executar(py.stdout)
            if erro or saida != esperado:
                problema = f"{nivel}: imprime {saida[:6]}, gcc {esperado[:6]}"
                break
        if problema:
            falhas += 1
            guardado = os.path.join(tmp, f"falha_{k}.c")
            with open(guardado, "w") as f:
                f.write(programa)
            print(f"[FALHA] programa {k}: {problema} ({guardado})")
    print(f"{quantidade} programas aleatorios, {falhas} falhas")
    return 1 if falhas else 0


if __name__ == "__main__":
    sys.setrecursionlimit(20000)
    sys.exit(main(sys.argv))
