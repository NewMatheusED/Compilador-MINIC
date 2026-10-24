float media(int v[], int n) {
    float s = 0;
    int i = 0;
    while (i < n) {
        s = s + v[i];
        i = i + 1;
    }
    return s / n;
}

int main() {
    int notas[4];
    char c = 'a';
    int codigo;
    notas[0] = 7;
    notas[1] = 8;
    notas[2] = 9;
    notas[3] = 5;
    print(media(notas, 4));
    codigo = c + 1;
    print(codigo);
    print(7 / 2);
    print(-7 % 3);
    return 0;
}
