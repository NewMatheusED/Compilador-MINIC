int chamadas = 0;

bool positivo(int x) {
    chamadas = chamadas + 1;
    return x > 0;
}

int main() {
    int a = 0;
    bool r;
    r = a != 0 && positivo(a);
    print(r);
    print(chamadas);
    r = a == 0 || positivo(a);
    print(r);
    print(chamadas);
    r = a == 0 && positivo(a + 1);
    print(r);
    print(chamadas);
    return 0;
}
