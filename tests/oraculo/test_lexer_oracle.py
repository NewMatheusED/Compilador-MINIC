import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "src", "lexer"))

from lexer import Lexer, TokenType  # noqa: E402


def tokenize(source):
    errors = []
    lx = Lexer(source, on_error=lambda msg: errors.append(msg))
    tokens = lx.tokenize()
    # representa cada token como (tipo, lexema, linha, coluna) pra comparar
    # facil com uma lista escrita a mao
    simplified = [(t.type, t.lexeme, t.line, t.col) for t in tokens]
    return simplified, lx.error_count


class TestLexerOracle(unittest.TestCase):

    def test_tokens_basicos_e_colunas(self):
        # "int x = 10;"
        #  123456789012
        # int(1-3) espaco(4) x(5) espaco(6) =(7) espaco(8) 10(9-10) ;(11)
        # EOF fica na coluna 12 (uma posicao depois do ultimo caractere)
        src = "int x = 10;"
        tokens, errors = tokenize(src)
        esperado = [
            (TokenType.KW_INT, "int", 1, 1),
            (TokenType.ID, "x", 1, 5),
            (TokenType.ASSIGN, "=", 1, 7),
            (TokenType.INT_LIT, "10", 1, 9),
            (TokenType.SEMI, ";", 1, 11),
            (TokenType.EOF, "", 1, 12),
        ]
        self.assertEqual(tokens, esperado)
        self.assertEqual(errors, 0)

    def test_operadores_de_dois_caracteres_nao_confundem_com_um(self):
        # "a <= b < c" -- o scanner precisa olhar 2 caracteres a frente pra
        # nao ler "<=" como "<" seguido de "=" (regra da secao 3.4/4.3).
        src = "a <= b < c"
        tokens, errors = tokenize(src)
        esperado = [
            (TokenType.ID, "a", 1, 1),
            (TokenType.LE, "<=", 1, 3),
            (TokenType.ID, "b", 1, 6),
            (TokenType.LT, "<", 1, 8),
            (TokenType.ID, "c", 1, 10),
            (TokenType.EOF, "", 1, 11),
        ]
        self.assertEqual(tokens, esperado)
        self.assertEqual(errors, 0)

    def test_ponto_sem_digito_nao_forma_real(self):
        src = "3.14 3. .5 42"
        tokens, errors = tokenize(src)
        esperado = [
            (TokenType.FLOAT_LIT, "3.14", 1, 1),
            (TokenType.INT_LIT, "3", 1, 6),
            (TokenType.INT_LIT, "5", 1, 10),
            (TokenType.INT_LIT, "42", 1, 12),
            (TokenType.EOF, "", 1, 14),
        ]
        self.assertEqual(tokens, esperado)
        self.assertEqual(errors, 2)  # os dois "." isolados

    def test_literais_de_caractere_validos_e_invalidos(self):
        # 'a'      -> caractere simples, valido
        # '\n'     -> escape valido (tabela da secao 3.5)
        # ''       -> vazio, tamanho invalido
        # 'ab'     -> dois caracteres, tamanho invalido
        src = r"'a' '\n' '' 'ab'"
        tokens, errors = tokenize(src)
        esperado = [
            (TokenType.CHAR_LIT, "'a'", 1, 1),
            (TokenType.CHAR_LIT, r"'\n'", 1, 5),
            (TokenType.CHAR_LIT, "''", 1, 10),
            (TokenType.CHAR_LIT, "'ab'", 1, 13),
            (TokenType.EOF, "", 1, 17),
        ]
        self.assertEqual(tokens, esperado)
        self.assertEqual(errors, 2)  # '' e 'ab'

    def test_palavra_reservada_e_identificador_que_comeca_igual(self):
        src = "int integer intx"
        tokens, errors = tokenize(src)
        esperado = [
            (TokenType.KW_INT, "int", 1, 1),
            (TokenType.ID, "integer", 1, 5),
            (TokenType.ID, "intx", 1, 13),
            (TokenType.EOF, "", 1, 17),
        ]
        self.assertEqual(tokens, esperado)
        self.assertEqual(errors, 0)

    def test_comentario_de_linha_nao_consome_a_quebra_de_linha(self):
        src = "a//comentario\nb"
        tokens, errors = tokenize(src)
        esperado = [
            (TokenType.ID, "a", 1, 1),
            (TokenType.ID, "b", 2, 1),
            (TokenType.EOF, "", 2, 2),
        ]
        self.assertEqual(tokens, esperado)
        self.assertEqual(errors, 0)


if __name__ == "__main__":
    unittest.main(verbosity=2)
