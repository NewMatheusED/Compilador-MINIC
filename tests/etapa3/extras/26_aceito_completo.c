int g = 10;
float media(int v[], int n) {
    float s = 0;
    int i = 0;
    while (i < n) {
        s = s + v[i];
        i = i + 1;
    }
    if (n == 0) return 0.0;
    return s / n;
}
bool par(int x) {
    return x % 2 == 0;
}
int principal() {
    int dados[4];
    char c;
    int k;
    float m;
    c = 'a';
    k = c + 1;
    dados[0] = k;
    dados[1] = g * 2;
    dados[2] = -dados[0];
    dados[3] = (1 + 2) * 3;
    m = media(dados, 4);
    if (par(k) || !(m > 1.5) && true) k = 0;
    else {
        int k;
        k = 2;
    }
    return k;
}
