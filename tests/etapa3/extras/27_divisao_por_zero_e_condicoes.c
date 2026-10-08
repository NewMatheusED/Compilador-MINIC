int principal() {
    int n;
    float f;
    n = 10 / 0;
    f = 1.0 / 0.0;
    n = n % 0;
    if (n + 1) n = 1;
    if (f) n = 2;
    while (n = 3) n = n - 1;
    return n;
}
