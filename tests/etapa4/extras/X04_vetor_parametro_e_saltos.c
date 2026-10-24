int soma_pares(int v[], int n) {
    int i;
    int total = 0;
    for (i = 0; i < n; i = i + 1) {
        if (v[i] % 2 != 0) continue;
        if (v[i] > 50) break;
        total = total + v[i];
    }
    return total;
}

int main() {
    int dados[6];
    int i;
    for (i = 0; i < 6; i = i + 1) {
        dados[i] = i * 12;
    }
    print(soma_pares(dados, 6));
    return 0;
}
