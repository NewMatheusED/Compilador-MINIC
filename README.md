# Compilador MINIC

Trabalho de Compiladores - Etapas 1, 2, 3 e IR (analisador léxico, analisador sintático, analisador semântico e representação intermediária com otimização).

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
- `minic.py` e `minic.c` - entrega da etapa 3 e da IR: diagnósticos semânticos e, com `--ir`, o código de três endereços
- `src/lexer/` - o lexer de verdade, em C e Python
- `src/parser/` - a AST e o parser, em C e Python
- `src/semantic/` - o analisador semântico, em C e Python
- `src/ir/` - o gerador de IR, em C e Python
- `src/optimizer/` - o otimizador do `-O1`, em C e Python
- `examples/` - programas `.mc` de exemplo, válidos e inválidos
- `tests/etapa1/` - casos e scripts de correção do léxico
- `tests/etapa2/` - os 50 casos e scripts do sintático
- `tests/etapa3/` - os 20 casos e scripts do semântico, mais 9 casos extras nossos em `tests/etapa3/extras/`
- `tests/etapa4/` - os 30 casos da IR do professor, 9 extras nossos, o interpretador de IR e o script de correção
- `tests/run_tests.sh` - compara a saída do C com a do Python

A pasta `src/codegen` continua vazia, reservada pra geração de código.

Na entrega passada eu tinha duas cópias do lexer, uma em `src/lexer/` e outra dentro de `scanner/`, porque o script de correção compila um arquivo só e eu não quis jogar fora a estrutura antiga. Isso ficou ruim de manter: qualquer correção tinha que ser feita em dois lugares. Agora o lexer está num lugar só, e o`scanner.c` e o `parser.c` da raiz só fazem `#include` dos módulos de `src/`. Continua compilando com um `gcc` só, como os scripts pedem, mas sem código duplicado. Em Python é a mesma ideia, os arquivos da raiz só importam de `src/`.

Também mudei o lexer pra avisar erro por callback em vez de imprimir direto. Assim o `cli.py` imprime no formato texto da especificação, o `scanner.py` imprime JSON, e o parser não imprime nada (porque a saída padrão é da AST).

## Como rodar

Python, sem compilar nada:

```
python scanner.py examples/validos/02_fatorial_recursivo.mc
python parser.py tests/etapa2/testes-parser-50/casos/25_programa_integrado/codigo.c
python minic.py tests/etapa3/minic-testes-semanticos/19_retorno_e_cobertura.c
python minic.py --ir tests/etapa4/minic-ir-test-cases/testes/01-validos-sem-otimizacao/P04/P04.c
python minic.py --ir -O1 tests/etapa4/minic-ir-test-cases/testes/03-otimizacoes/O07/O07.c
```

C:

```
gcc -Wall -Wextra -std=c11 scanner.c -o scanner
gcc -Wall -Wextra -std=c11 parser.c -o parser
gcc -Wall -Wextra -std=c11 minic.c -o minic
./scanner examples/validos/02_fatorial_recursivo.mc
./parser tests/etapa2/testes-parser-50/casos/25_programa_integrado/codigo.c
./minic tests/etapa3/minic-testes-semanticos/19_retorno_e_cobertura.c
./minic --ir -O1 tests/etapa4/extras/X01_invariante_no_laco.c
```

Com `make` instalado dá pra usar `make`, `make scanner`, `make parser`, `make minic` e `make test`.

Opções do `minic` (seção 11 da especificação):

- sem opção ou `--check` - só as análises, imprime os diagnósticos semânticos (igual na etapa 3)
- `--ir` - imprime a IR, se o programa passar em todas as análises
- `-O0` (padrão) e `-O1` - desliga/liga o otimizador

Pra ver os tokens em texto durante o desenvolvimento, que foi como eu comecei na etapa 1, o driver antigo continua lá:

```
python src/lexer/cli.py --tokens examples/validos/02_fatorial_recursivo.mc
```

## Testes

Continuo usando WSL, por isso os `.sh`. Nos computadores da faculdade dá pra rodar pelo Git Bash.

```
make test
```

Isso roda cinco coisas: a comparação C contra Python em cima de `examples/`, os scripts de correção do léxico, os 50 casos do sintático, os scripts de correção do semântico (casos do professor e os extras) e a correção da IR.

Como está agora:

- comparação C x Python: 14 passaram, 0 falharam
- etapa 1, `scanner.py`: 13 OK, 0 falharam
- etapa 1, `scanner.c`: 13 OK, 0 falharam
- etapa 2, `parser.py`: 49 OK, 1 problema de gabarito
- etapa 2, `parser.c`: 49 OK, 1 problema de gabarito
- etapa 3, `minic.py`: 20/20 aprovados (e 9/9 nos extras)
- etapa 3, `minic.c`: 20/20 aprovados (e 9/9 nos extras)
- IR, `minic.py`: 10/10 normais, 10/10 erros, 10/10 otimização, 9/9 extras
- IR, `minic.c`: 10/10 normais, 10/10 erros, 10/10 otimização, 9/9 extras

O C e o Python dão exatamente a mesma saída nos 50 casos do sintático e nos 29 do semântico, byte a byte. Na IR também: 68 programas, em `-O0` e `-O1`.

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
comando     -> bloco | se | enquanto | para | retorno | break | continue
             | print | read | decl_var | expr ';'
bloco       -> '{' comando* '}'
se          -> 'if' '(' expr ')' comando ('else' comando)?
enquanto    -> 'while' '(' expr ')' comando
para        -> 'for' '(' expr? ';' expr? ';' expr? ')' comando
retorno     -> 'return' expr? ';'
break       -> 'break' ';'
continue    -> 'continue' ';'
print       -> 'print' '(' expr ')' ';'
read        -> 'read' '(' expr ')' ';'

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

No `scanner` e no `parser` continua como antes:

- `0` - analisou sem erro
- `1` - uso errado, arquivo não encontrado, ou erro sintático no parser
- `2` - erro léxico no scanner

O `minic` agora segue a tabela da seção 11.1 da especificação, que é o que os gabaritos da IR pedem:

- `0` - sucesso
- `1` - uso errado ou arquivo não encontrado
- `2` - erro léxico
- `3` - erro sintático
- `4` - erro semântico

Na etapa 3 o semântico saía com 3 e o léxico com 1. Mudei porque o E09 espera 3 pra erro sintático e o E10 espera 2 pra léxico. Os scripts da etapa 3 não olham o código de saída, então continuam 20/20. Pra separar léxico de sintático o `minic` agora passa o lexer no arquivo inteiro antes do parser; antes, no `int x = 1 @ 2;` do E10 o parser chegava primeiro no `2` e reclamava de `;`.

## IR

A IR é código de três endereços, no mesmo formato da "IR canônica" dos gabaritos do professor:

```
func combinar(x, y)
  t1 = x * 10
  t2 = t1 + y
  return t2
endfunc
func main
  param 3
  param 8
  t1 = call combinar, 2
  r = t1
  print r
  return 0
endfunc
```

As instruções (cobrem as operações mínimas da seção 9.1):

| instrução                                                      | operação da especificação                |
| -------------------------------------------------------------- | ---------------------------------------- |
| `x = a + b` (também `- * / %`, `< <= > >= == !=`, `and`, `or`) | ADD, SUB, MUL, DIV, MOD                  |
| `x = neg a`, `x = not a`, `x = itof a`                         | NEG, NOT, conversão int -> float         |
| `x = a`                                                        | ASSIGN                                   |
| `x = load v[i]`, `store v[i], a`                               | LOAD, STORE                              |
| `L:`, `goto L`, `ifFalse a goto L`, `ifTrue a goto L`          | LABEL, JUMP, JUMP_IF_FALSE, JUMP_IF_TRUE |
| `param a`, `x = call f, n` (ou `call f, n` se for void)        | PARAM, CALL                              |
| `return a`, `print a`, `read x`                                | RETURN, saída e entrada                  |
| `array v[n]`, `global g`, `global v[n]`                        | declaração de vetor local e de global    |

Decisões que tomei onde a especificação não fixa nada:

- temporários e rótulos recomeçam em cada função (`t1`, `L_else`, `L_end`, `L_test`... e `L_end2` se repetir), como nos gabaritos
- variável que sombreia outra ganha sufixo: no P09 fica `x` e `x#2`, onde o gabarito usa `x#outer` e `x#inner`
- a memória da IR é endereçada por elemento, então o deslocamento é `i * 1`, igual ao P07. Diferente do gabarito, eu declaro o vetor com `array v[3]`, porque a geração de código vai precisar do tamanho
- `&&` e `||` viram `and`/`or` direto quando o lado direito não tem chamada, atribuição, índice ou divisão (como no P08); se tiver, gera curto-circuito com salto, senão `a != 0 && f(a)` chamaria `f` sempre
- int indo pra float (inicialização, atribuição, argumento, retorno, conta mista) vira `itof`, ou `7.0` direto se for literal
- variáveis globais e comandos no nível global ficam num bloco `init ... endinit` antes das funções, que roda antes da `main`
- função `void` sem `return` no final ganha um `return`

### Otimizações (`-O1`)

O otimizador roda os passos abaixo em sequência, repetindo até nada mudar:

1. dobra de constantes e identidades algébricas (`x + 0`, `x * 1`, `x * 0`, `true and x`...), e `ifFalse` com condição constante vira `goto` ou some (local)
2. propagação de cópias e de constantes por análise de fluxo de dados no grafo de blocos básicos (cópias disponíveis, que é a forma "de cópia" do reaching definitions) (global)
3. eliminação de subexpressão comum dentro do bloco (local)
4. eliminação de código morto por análise de variáveis vivas (global)
5. `t = a + b; x = t` vira `x = a + b` quando `t` só é usado ali
6. remoção de bloco inalcançável, de `goto` pro rótulo seguinte e de rótulo sem uso
7. movimento de código invariante pra antes do laço, achando os laços pelos dominadores (global)

Chamada de função pode mexer em global, então ela invalida o que se sabia sobre as globais. Divisão e `%` não saem do laço, porque se o laço não rodar nenhuma vez o programa passaria a poder dividir por zero. Redução de força (O09) eu não fiz, o README do pacote diz que não é obrigatória.

Os casos O01 a O10 do professor quase todos viram `print <constante>`, porque os valores são todos conhecidos. Pra mostrar o CSE e o movimento de invariante de verdade, os extras X01 e X02 fazem a mesma conta com parâmetros:

```
python minic.py --ir -O1 tests/etapa4/extras/X01_invariante_no_laco.c
```

### Testes da IR

```
python3 tests/etapa4/testar_ir.py            # minic.py
python3 tests/etapa4/testar_ir.py --c        # minic.c
python3 tests/etapa4/testar_ir.py --paridade # C x Python byte a byte
python3 tests/etapa4/comparar_com_gcc.py 1 200
```

O `tests/etapa4/ir_interp.py` é um interpretador da IR textual. Com ele dá pra executar a nossa IR e também a IR de referência que está dentro dos gabaritos, e comparar o que cada uma imprime. O `testar_ir.py` segue os critérios do README do pacote:

- válidos: a IR `-O0`, depois de normalizar nomes de temporários, rótulos e variáveis sombreadas, é igual ao gabarito (P01 a P05, P08 e P09) ou imprime a mesma coisa que a IR do gabarito quando as duas são executadas (P06, P07 e P10); e a `-O1` imprime o mesmo que a `-O0`
- erros: código de saída igual ao da categoria do gabarito e nenhuma IR na saída
- otimização: `-O0` e `-O1` imprimem o mesmo e a `-O1` não tem mais instruções que a "IR otimizada" do gabarito
- extras: `-O0` e `-O1` imprimem o `.saida` (no X07 lendo o `.entrada`)

Os três que ficam como "equivalente" e não "igual": no P06 o gabarito põe `i = 1` antes de `produto = 1`, mas no fonte `produto` é inicializado primeiro; no P07 a diferença é a linha `array v[3]`; no P10 o gabarito tem um `goto L_end` depois do `return 1`, que não aparece no P08, que é a mesma estrutura de `if` sem `else`.

O `comparar_com_gcc.py` gera programas aleatórios, compila cada um com o gcc e confere se a nossa IR `-O0` e `-O1` imprimem o mesmo que o executável do gcc, e se o C e o Python geram a mesma IR. Rodei com algumas centenas de programas sem nenhuma diferença.

### Sobre os scripts `test_ir_c.sh` e `test_ir_python.sh`

Eles estão no pacote sem alteração, mas dão 0/30 com qualquer compilador. Eles chamam `minic arquivo.c` sem `--ir` e comparam a saída byte a byte com o `.txt` inteiro, só tirando linhas que começam com "erro", "aviso" e parecidos. Só que o `.txt` não é uma saída de compilador: ele tem linhas como `Resultado: REJEITAR`, `Categoria esperada: erro semântico (código de saída 4)` e `Critério: diagnóstico de declaração duplicada...`, e nos casos de otimização tem a IR `-O0` e a `-O1` no mesmo arquivo. O próprio README do pacote diz que os gabaritos são "uma IR canônica de referência, não uma exigência de igualdade byte a byte" e pede pra comparar estrutura e semântica, que é o que o `testar_ir.py` faz.
