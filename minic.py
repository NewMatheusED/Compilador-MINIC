import os
import sys

_RAIZ = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(_RAIZ, "src", "lexer"))
sys.path.insert(0, os.path.join(_RAIZ, "src", "parser"))
sys.path.insert(0, os.path.join(_RAIZ, "src", "semantic"))

from minic_parser import ErroSintatico, Parser  # noqa: E402
from minic_semantic import analisar  # noqa: E402


def escrever(texto, fluxo):
    # no Windows o print usa cp1252 e troca \n por \r\n
    fluxo.flush()
    fluxo.buffer.write(texto.encode("utf-8"))
    fluxo.buffer.flush()


def main(argv):
    if len(argv) < 2:
        escrever("uso: minic.py arquivo.c\n", sys.stderr)
        return 1

    caminho = argv[1]
    try:
        with open(caminho, "r", encoding="utf-8") as f:
            fonte = f.read()
    except (OSError, UnicodeDecodeError):
        escrever(f'erro: nao foi possivel abrir o arquivo "{caminho}"\n',
                 sys.stderr)
        return 1

    try:
        arvore = Parser(fonte).analisar()
    except ErroSintatico as e:
        escrever(e.formatar() + "\n", sys.stderr)
        return 1

    diagnosticos, saida = analisar(arvore)
    escrever(saida, sys.stdout)
    return 3 if diagnosticos else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
