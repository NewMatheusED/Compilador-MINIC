import sys

from lexer import Lexer, TokenType, formatar_erro_lexico


def main(argv):
    show_tokens = False
    path = None

    for arg in argv[1:]:
        if arg == "--tokens":
            show_tokens = True
        else:
            path = arg

    if path is None:
        print("uso: cli.py [--tokens] arquivo.mc", file=sys.stderr)
        return 1

    try:
        with open(path, "r", encoding="utf-8") as f:
            source = f.read()
    except OSError:
        print(f'erro: nao foi possivel abrir o arquivo "{path}"', file=sys.stderr)
        return 1

    def on_error(msg, line, col):
        print(formatar_erro_lexico(msg, line, col), end="")

    lx = Lexer(source, on_error=on_error)

    token_count = 0
    while True:
        tok = lx.next_token()
        token_count += 1
        if show_tokens:
            print(f'{tok.line}:{tok.col}\t{tok.type.name}\t"{tok.lexeme}"')
        if tok.type == TokenType.EOF:
            break

    if show_tokens:
        print(f"total de tokens: {token_count}", file=sys.stderr)
        print(f"total de erros lexicos: {lx.error_count}", file=sys.stderr)

    return 2 if lx.error_count > 0 else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
