int soma_produtos(int x, int y, int n) {
    int i = 0;
    int soma = 0;
    while (i < n) {
        soma = soma + x * y;
        i = i + 1;
    }
    return soma;
}

int main() {
    print(soma_produtos(6, 7, 4));
    return 0;
}
