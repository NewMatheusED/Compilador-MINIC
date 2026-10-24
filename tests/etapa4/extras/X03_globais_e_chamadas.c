int contador = 10;
int limite = contador * 2;

void incrementa(int passo) {
    contador = contador + passo;
}

int main() {
    int antes = contador;
    incrementa(5);
    print(antes);
    print(contador);
    print(limite);
    return 0;
}
