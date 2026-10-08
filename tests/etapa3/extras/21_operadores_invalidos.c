int principal() {
    int n;
    bool b;
    float f;
    n = 1;
    b = n < 2;
    f = 1.5;
    n = n + b;
    b = !n;
    b = n && b;
    n = f % 2;
    n = -b;
    b = b == n;
    return 0;
}
