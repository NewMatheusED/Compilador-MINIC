# Compilador MINIC

Trabalho de Compiladores - Etapas 1, 2 e 3 (analisador léxico, analisador sintático e analisador semântico).

## Integrantes

- Matheus Eduardo - 2400866
- Gustavo Boschini - 2401529
- Gabriel Martins - 2401250
- Vinicius Dias - 2401453
- Rafael Nesterur - 2401203
- Luisa de Souza - 2401104

## Estrutura

- `scanner.py` e `scanner.c` - entrega da etapa 1, saída em JSON Lines
- `parser.py` e `parser.c` - entrega da etapa 2, saída com a AST
- `minic.py` e `minic.c` - entrega da etapa 3, saída com os diagnósticos semânticos
- `src/lexer/` - o lexer de verdade, em C e Python
- `src/parser/` - a AST e o parser, em C e Python
- `src/semantic/` - o analisador semântico, em C e Python
- `examples/` - programas `.mc` de exemplo, válidos e inválidos
- `tests/etapa1/` - casos e scripts de correção do léxico
- `tests/etapa2/` - os 50 casos e scripts do sintático
- `tests/etapa3/` - os 20 casos e scripts do semântico, mais 9 casos extras nossos em `tests/etapa3/extras/`
- `tests/run_tests.sh` - compara a saída do C com a do Python

As pastas `src/ir`, `src/codegen` e `src/optimizer` continuam vazias, reservadas pras próximas etapas.

Na entrega passada eu tinha duas cópias do lexer, uma em `src/lexer/` e outra dentro de `scanner/`, porque o script de correção compila um arquivo só e eu não quis jogar fora a estrutura antiga. Isso ficou ruim de manter: qualquer correção tinha que ser feita em dois lugares. Agora o lexer está num lugar só, e o`scanner.c` e o `parser.c` da raiz só fazem `#include` dos módulos de `src/`. Continua compilando com um `gcc` só, como os scripts pedem, mas sem código duplicado. Em Python é a mesma ideia, os arquivos da raiz só importam de `src/`.

Também mudei o lexer pra avisar erro por callback em vez de imprimir direto. Assim o `cli.py` imprime no formato texto da especificação, o `scanner.py` imprime JSON, e o parser não imprime nada (porque a saída padrão é da AST).

## Como rodar

Python, sem compilar nada:

```
python scanner.py examples/validos/02_fatorial_recursivo.mc
python parser.py tests/etapa2/testes-parser-50/casos/25_programa_integrado/codigo.c
python minic.py tests/etapa3/minic-testes-semanticos/19_retorno_e_cobertura.c
```

C:

```
gcc -Wall -Wextra -std=c11 scanner.c -o scanner
gcc -Wall -Wextra -std=c11 parser.c -o parser
gcc -Wall -Wextra -std=c11 minic.c -o minic
./scanner examples/validos/02_fatorial_recursivo.mc
./parser tests/etapa2/testes-parser-50/casos/25_programa_integrado/codigo.c
./minic tests/etapa3/minic-testes-semanticos/19_retorno_e_cobertura.c
```

Com `make` instalado dá pra usar `make`, `make scanner`, `make parser`, `make minic` e `make test`.

Pra ver os tokens em texto durante o desenvolvimento, que foi como eu comecei na etapa 1, o driver antigo continua lá:

```
python src/lexer/cli.py --tokens examples/validos/02_fatorial_recursivo.mc
```

## Testes

Continuo usando WSL, por isso os `.sh`. Nos computadores da faculdade dá pra rodar pelo Git Bash.

```
make test
```

Isso roda quatro coisas: a comparação C contra Python em cima de `examples/`, os scripts de correção do léxico, os 50 casos do sintático e os scripts de correção do semântico (casos do professor e os extras).

Como está agora:

- comparação C x Python: 14 passaram, 0 falharam
- etapa 1, `scanner.py`: 13 OK, 0 falharam
- etapa 1, `scanner.c`: 13 OK, 0 falharam
- etapa 2, `parser.py`: 49 OK, 1 problema de gabarito
- etapa 2, `parser.c`: 49 OK, 1 problema de gabarito
- etapa 3, `minic.py`: 20/20 aprovados (e 9/9 nos extras)
- etapa 3, `minic.c`: 20/20 aprovados (e 9/9 nos extras)

O C e o Python dão exatamente a mesma saída nos 50 casos do sintático e nos 29 do semântico, byte a byte.

Os scripts da etapa 3 são os do professor, sem alteração:

```
./tests/etapa3/testes_semanticos_py.sh minic.py tests/etapa3/minic-testes-semanticos
./tests/etapa3/testes_semanticos_c.sh minic.c tests/etapa3/minic-testes-semanticos
```

Os 7 "avisos" na etapa 1 são os casos de erro léxico, onde o scanner sai com código 2 como manda a seção 11.1 da especificação. O script conta isso como aviso e termina com código 1 mesmo com zero falhas, por isso o `make` ignora o código de saída dele.

## Sobre os testes da etapa 2

Os scripts `testar_parser_c.sh` e `testar_parser_python.sh` comparam a saída byte a byte com o `ast.esperada.txt`. Rodando eles direto eu fico em 16 de 50, mas não é porque o parser esteja errado. Achei três coisas:

**1) Os 25 casos inválidos comparam contra a frase, não contra o diagnóstico.**
O find_expected dos scripts acaba escolhendo o `ast.esperada.txt`, que nos casos 26 a 50 contém `NÃO HÁ AST: o parser deve rejeitar a entrada.`. Pra "passar" eu teria que imprimir essa frase exata em vez da mensagem de erro. O README do próprio pacote de testes pede o contrário, código de saída diferente de zero e uma mensagem de erro sintático, e diz que a mensagem pode variar entre implementações.

**2) Oito gabaritos usam espaçamento diferente dos outros.** 
O caso 02 espera `VarDecl(int x = Lit(int,42))` com espaço no `=`, e o caso 12 espera `VarDecl(bool ok=Binary(...))` sem espaço. O caso 06 espera `Binary(+, Id(a), Id(b))` com espaço depois da vírgula, e o caso 12 espera `Binary(<,Id(i),Lit(int,10))` sem. Não tem como o mesmo impressor acertar os dois estilos. Eu segui o estilo da maioria e deixei a convenção escrita em `src/parser/minic_ast.py`. Os casos afetados são 02, 03, 04, 06, 07, 08, 09 e 10, e a diferença é só espaço em branco.

**3) O gabarito do caso 24 está quebrado.** 
A string tem 23 parênteses abrindo e 24 fechando, então nem é uma S-expressão válida. Além disso o Return aparece como irmão do Block dentro do Function, mas no código-fonte o return i está dentro do corpo da função, junto com o while. O esperado deveria terminar com o Return dentro do Block.

Por isso escrevi o `tests/etapa2/testar_parser.sh`, que compara ignorando espaço em branco nos casos válidos e usa o critério do README (saída diferente de zero mais mensagem de erro) nos inválidos. Os dois scripts originais estão no repositório sem nenhuma alteração, é só rodar se quiser conferir.

Se quiser rodar o script original mesmo assim, coloquei uma variável de ambiente que faz o parser imprimir a frase esperada nos casos rejeitados:

```
MODO_SCRIPT=1 bash tests/etapa2/testar_parser_python.sh tests/etapa2/testes-parser-50 ./parser.py
```

Aí ele vai de 16 pra 41 de 50, e as 9 que sobram são exatamente os 8 casos de espaçamento e o caso 24.

## A gramática que o parser aceita

```
programa    -> item*
item        -> funcao | decl_var | comando
funcao      -> TIPO IDENT '(' params ')' bloco
params      -> vazio | param (',' param)*
param       -> TIPO IDENT ('[' ']')?
decl_var    -> TIPO IDENT ('[' expr ']')? ('=' expr)? ';'
comando     -> bloco | se | enquanto | retorno | decl_var | expr ';'
bloco       -> '{' comando* '}'
se          -> 'if' '(' expr ')' comando ('else' comando)?
enquanto    -> 'while' '(' expr ')' comando
retorno     -> 'return' expr? ';'

expr        -> atribuicao
atribuicao  -> ou ('=' atribuicao)?
ou          -> e ('||' e)*
e           -> igualdade ('&&' igualdade)*
igualdade   -> relacional (('=='|'!=') relacional)*
relacional  -> aditiva (('<'|'<='|'>'|'>=') aditiva)*
aditiva     -> multiplicativa (('+'|'-') multiplicativa)*
multipl.    -> unaria (('*'|'/'|'%') unaria)*
unaria      -> ('-'|'!'|'+') unaria | posfixa
posfixa     -> primaria ('(' args ')' | '[' expr ']')*
primaria    -> IDENT | literal | 'true' | 'false' | '(' expr ')'
```

Três decisões que eu tomei olhando os 50 casos, caso seja diferente do esperado:

- o nível global aceita comando, não só declaração, por causa do caso 22, que tem `a = b = 3;` fora de qualquer função
- função sem corpo é erro, então protótipo não entra, por causa do caso 48
- o `=` aceita qualquer lado esquerdo, e quem verifica se ele é atribuível é o semântico (SEM013). Na etapa 2 eu tinha restringido a `Id`/`Index` no parser, mas o teste 20 da etapa 3 (`3 = n;`) espera erro semântico, não sintático. O caso 41, `a[1 = 2;`, continua reclamando de `esperado FECHA_COLCHETE`, como a pista pede, só que agora no `;`

Na etapa 3 o parser também passou a aceitar parâmetro vetor (`int dados[]`, previsto na especificação e usado nos testes 04, 07 e 10) e todo nó da AST guarda linha e coluna de onde começa, pros diagnósticos. A impressão da AST não mudou.

## Códigos de saída

- `0` - analisou sem erro
- `1` - uso errado, arquivo não encontrado, ou erro sintático no parser (no `minic`, também erro léxico)
- `2` - erro léxico no scanner
- `3` - programa rejeitado pela análise semântica (só no `minic`)
