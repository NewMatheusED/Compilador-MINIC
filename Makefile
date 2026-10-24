CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -O2

# Entregaveis de arquivo unico exigidos pelos scripts de correcao.
# Ambos apenas incluem os modulos reais de src/ -- nao ha codigo duplicado.
BIN_SCANNER = bin/scanner
BIN_PARSER  = bin/parser
BIN_MINIC   = bin/minic

# Driver de desenvolvimento do lexer (saida textual, flag --tokens).
BIN_LEXER   = bin/minic_lexer
SRC_LEXER   = src/lexer/lexer.c src/lexer/main.c

.PHONY: all scanner parser minic lexer test test-etapa1 test-etapa2 test-etapa3 test-etapa4 test-regressao clean

all: scanner parser minic lexer

scanner: $(BIN_SCANNER)
parser:  $(BIN_PARSER)
minic:   $(BIN_MINIC)
lexer:   $(BIN_LEXER)

$(BIN_SCANNER): scanner.c src/lexer/lexer.c src/lexer/lexer.h
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ scanner.c

$(BIN_PARSER): parser.c src/lexer/lexer.c src/lexer/lexer.h \
               src/parser/parser.c src/parser/parser.h \
               src/parser/ast.c src/parser/ast.h
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ parser.c

$(BIN_MINIC): minic.c src/lexer/lexer.c src/lexer/lexer.h \
              src/parser/parser.c src/parser/parser.h \
              src/parser/ast.c src/parser/ast.h \
              src/semantic/semantic.c src/semantic/semantic.h \
              src/ir/ir.c src/ir/ir.h \
              src/optimizer/opt.c src/optimizer/opt.h
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ minic.c

$(BIN_LEXER): $(SRC_LEXER) src/lexer/lexer.h
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $(SRC_LEXER)

# Testes -------------------------------------------------------------------

test: test-regressao test-etapa1 test-etapa2 test-etapa3 test-etapa4

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

# Scripts de correcao do professor para o semantico, mais os nossos extras
test-etapa3: $(BIN_MINIC)
	bash tests/etapa3/testes_semanticos_py.sh minic.py tests/etapa3/minic-testes-semanticos
	bash tests/etapa3/testes_semanticos_c.sh minic.c tests/etapa3/minic-testes-semanticos
	bash tests/etapa3/testes_semanticos_py.sh minic.py tests/etapa3/extras
	bash tests/etapa3/testes_semanticos_c.sh minic.c tests/etapa3/extras

# IR: casos do professor pelo criterio do README do pacote, mais paridade C x Python
test-etapa4: $(BIN_MINIC)
	python3 tests/etapa4/testar_ir.py
	python3 tests/etapa4/testar_ir.py --c
	python3 tests/etapa4/testar_ir.py --paridade

clean:
	rm -rf bin scanner parser
	find . -name '__pycache__' -type d -exec rm -rf {} +
