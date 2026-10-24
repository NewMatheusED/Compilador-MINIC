import os
import sys

_RAIZ = os.path.dirname(os.path.abspath(__file__))
for _pasta in ("lexer", "parser", "semantic", "ir", "optimizer"):
    sys.path.insert(0, os.path.join(_RAIZ, "src", _pasta))

from lexer import Lexer, TokenType, formatar_erro_lexico  # noqa: E402
from minic_parser import ErroSintatico, Parser  # noqa: E402
from minic_semantic import analisar  # noqa: E402
from minic_ir import gerar_ir  # noqa: E402
from minic_opt import otimizar  # noqa: E402

# codigos de saida da secao 11.1 da especificacao
OK, USO, LEXICO, SINTATICO, SEMANTICO = 0, 1, 2, 3, 4

USO_TEXTO = "uso: minic.py [--check | --ir] [-O0 | -O1] arquivo.c\n"


def escrever(texto, fluxo):
    # no Windows o print usa cp1252 e troca \n por \r\n
    fluxo.flush()
    fluxo.buffer.write(texto.encode("utf-8"))
    fluxo.buffer.flush()


def erros_lexicos(fonte):
    erros = []
    lx = Lexer(fonte, on_error=lambda m, l, c: erros.append(
        formatar_erro_lexico(m, l, c)))
    while lx.next_token().type != TokenType.EOF:
        pass
    return erros


def main(argv):
    modo_ir = False
    otimizacao = 0
    arquivos = []
    for arg in argv[1:]:
        if arg == "--ir":
            modo_ir = True
        elif arg == "--check":
            modo_ir = False
        elif arg in ("-O0", "-O1"):
            otimizacao = int(arg[2])
        elif arg.startswith("-"):
            escrever(f"opcao desconhecida: {arg}\n" + USO_TEXTO, sys.stderr)
            return USO
        else:
            arquivos.append(arg)
    if len(arquivos) != 1:
        escrever(USO_TEXTO, sys.stderr)
        return USO

    caminho = arquivos[0]
    try:
        with open(caminho, "r", encoding="utf-8") as f:
            fonte = f.read()
    except (OSError, UnicodeDecodeError):
        escrever(f'erro: nao foi possivel abrir o arquivo "{caminho}"\n',
                 sys.stderr)
        return USO

    lexicos = erros_lexicos(fonte)
    if lexicos:
        escrever("".join(lexicos), sys.stderr)
        return LEXICO

    try:
        arvore = Parser(fonte).analisar()
    except ErroSintatico as e:
        escrever(e.formatar() + "\n", sys.stderr)
        return SINTATICO

    diagnosticos, saida = analisar(arvore)
    if diagnosticos or not modo_ir:
        escrever(saida, sys.stdout)
        return SEMANTICO if diagnosticos else OK

    ir = gerar_ir(arvore)
    if otimizacao:
        otimizar(ir)
    escrever(ir.texto(), sys.stdout)
    return OK


if __name__ == "__main__":
    sys.exit(main(sys.argv))
