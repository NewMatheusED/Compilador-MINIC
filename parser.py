import os
import sys

_RAIZ = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(_RAIZ, "src", "lexer"))
sys.path.insert(0, os.path.join(_RAIZ, "src", "parser"))

from minic_parser import ErroSintatico, Parser  # noqa: E402

FRASE_SEM_AST = "NÃO HÁ AST: o parser deve rejeitar a entrada."


def main(argv):
    if len(argv) < 2:
        print("uso: parser.py arquivo.c", file=sys.stderr)
        return 1

    caminho = argv[1]
    try:
        with open(caminho, "r", encoding="utf-8") as f:
            fonte = f.read()
    except OSError:
        print(f'erro: nao foi possivel abrir o arquivo "{caminho}"',
              file=sys.stderr)
        return 1

    try:
        arvore = Parser(fonte).analisar()
    except ErroSintatico as e:
        if os.environ.get("MODO_SCRIPT") == "1":
            print(FRASE_SEM_AST)
        else:
            print(e.formatar(), file=sys.stderr)
        return 1

    print(arvore.render())
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
