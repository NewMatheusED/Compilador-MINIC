# Compilador MINIC - Analisador Léxico

Trabalho de Compiladores - Etapa 1.

## Integrantes
- Matheus Eduardo - 2400866
- Gustavo Boschini - 2401529
- Gabriel Martins - 2401250
- Vinicius Dias - 2401453
- Rafael Nesterur - 2401203
- Luisa de Souza - 2401104

## Estrutura

- `src/lexer/` - código do lexer (C e Python)
- `examples/validos/` e `examples/invalidos/` - programas de teste
- `tests/` - script que roda os testes e os resultados obtidos
- `scanner/` - mesma coisa, mas no formato exigido pelo script de correção que o professor mandou (`scanner.c`/`scanner.py` únicos, saída em JSON Lines, testes em `.minic`). Ver `scanner/README.md`.

As pastas `src/parser`, `src/semantic`, `src/ir` etc. estão vazias por enquanto, reservadas para as próximas etapas do trabalho.

## Como rodar

Python (não precisa compilar nada):

```
python src/lexer/cli.py --tokens examples/validos/02_fatorial_recursivo.mc
```

C:

```
gcc -o bin/minic_lexer src/lexer/lexer.c src/lexer/main.c
bin/minic_lexer --tokens examples/validos/02_fatorial_recursivo.mc
```

Se tiver `make` instalado, dá pra usar `make` e `make test` em vez dos
comandos acima.

## Testes

Estou usando ambiente WSL, por isso o sh. Mas, caso não tenha, dá pra rodar o script de testes no Windows com Git Bash, sei que tem nos computadores da faculdade.

```
bash tests/run_tests.sh
```

O script roda o lexer em C e em Python sobre todos os arquivos de `examples/` e compara a saída dos dois com o resultado esperado, que fica salvo em `tests/results/`. No momento os 12 casos de teste passam.

Para conferir os resultados reais, basta rodar os arquivos em `examples/`. O arquivo possui 2 principais teste, um Oraculo Teste e um Teste de Comparacao. Optei fazer os dois para conferir tanto a saída do lexer em C quanto a saída do lexer em Python. O Oraculo Teste é o resultado esperado, e o Teste de Comparacao é a saída real do lexer.

## Códigos de saída

- `0` - compilou sem erro léxico
- `1` - uso errado (arquivo não encontrado, etc)
- `2` - erro léxico encontrado

## Scanner

Tive que implementar isso logo após sua aula do dia 20/08.
A estrutura que tinha feito era baseada em flag e retornando texto, o script que foi enviado exige JSON... fiquei com do de apagar os testes e a estrutura antiga que tinha feito, então fiz um scanner separado, que está na pasta `scanner/`.
De qualquer forma, espero um retorno na tarefa para ver qual prefere manter, a logica do lexer em C e Python é a mesma, só muda a saída.
