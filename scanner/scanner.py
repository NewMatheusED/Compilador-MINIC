import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from lexer import Lexer, TokenType  # noqa: E402


def main(argv):
    if len(argv) < 2:
        print("uso: scanner.py arquivo.c|arquivo.minic", file=sys.stderr)
        return 1

    path = argv[1]
    try:
        with open(path, "r", encoding="utf-8") as f:
            source = f.read()
    except OSError:
        print(f'erro: nao foi possivel abrir o arquivo "{path}"', file=sys.stderr)
        return 1

    def on_error(msg, line, col):
        print(json.dumps({"error": "lexico", "message": msg, "line": line, "column": col}))

    lx = Lexer(source, on_error=on_error)

    while True:
        tok = lx.next_token()
        print(json.dumps({
            "token": tok.type.name,
            "lexeme": tok.lexeme,
            "line": tok.line,
            "column": tok.col,
        }))
        if tok.type == TokenType.EOF:
            break

    return 2 if lx.error_count > 0 else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
