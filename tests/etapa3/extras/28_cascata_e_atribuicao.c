int acao() {
    return 0;
}
void faz() {
    return;
}
int principal() {
    int n;
    float f;
    bool b;
    n = x + y;
    n = f = 2;
    f = n = 2.5;
    b = faz() + 1;
    n + 1 = 2;
    acao() = 1;
    (n = 1) = 2;
    acao(faz());
    if (faz()) n = 1;
    return faz();
}
