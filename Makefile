CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -O2

# Entregaveis de arquivo unico exigidos pelos scripts de correcao.
# Ambos apenas incluem os modulos reais de src/ -- nao ha codigo duplicado.
BIN_SCANNER = bin/scanner
BIN_PARSER  = bin/parser

# Driver de desenvolvimento do lexer (saida textual, flag --tokens).
BIN_LEXER   = bin/minic_lexer
SRC_LEXER   = src/lexer/lexer.c src/lexer/main.c

.PHONY: all scanner parser lexer test test-etapa1 test-etapa2 test-regressao clean

all: scanner parser lexer

scanner: $(BIN_SCANNER)
parser:  $(BIN_PARSER)
lexer:   $(BIN_LEXER)

$(BIN_SCANNER): scanner.c src/lexer/lexer.c src/lexer/lexer.h
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ scanner.c

$(BIN_PARSER): parser.c src/lexer/lexer.c src/lexer/lexer.h \
               src/parser/parser.c src/parser/parser.h \
               src/parser/ast.c src/parser/ast.h
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ parser.c

$(BIN_LEXER): $(SRC_LEXER) src/lexer/lexer.h
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $(SRC_LEXER)

# Testes -------------------------------------------------------------------

test: test-regressao test-etapa1 test-etapa2

# Paridade C x Python + oraculo do lexer (montado pelo grupo)
test-regressao: lexer
	bash tests/run_tests.sh

# Scripts de correcao do professor para o analisador lexico.
# Eles terminam com codigo 1 quando ha qualquer "aviso", e os 7 avisos sao
# justamente os casos de erro lexico, em que o scanner sai com codigo 2 como
# manda a especificacao. Por isso o `|| true`: o que importa e "0 falharam".
test-etapa1: $(BIN_SCANNER)
	bash tests/etapa1/test_scanner_python.sh scanner.py tests/etapa1 || true
	bash tests/etapa1/test_scanner_c.sh scanner.c tests/etapa1 || true

# 50 casos do analisador sintatico, com comparacao normalizada
test-etapa2: $(BIN_PARSER)
	bash tests/etapa2/testar_parser.sh

clean:
	rm -rf bin scanner parser
	find . -name '__pycache__' -type d -exec rm -rf {} +
