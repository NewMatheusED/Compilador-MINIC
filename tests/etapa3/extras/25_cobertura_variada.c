int a(bool c) {
    while (c) return 1;
}
int b(bool c) {
    if (c) { if (c) return 1; } else return 2;
}
int d(bool c) {
    if (c) c = c; else c = c;
}
float e() {
}
int f(int n) {
    int r;
    r = n;
}
int principal() {
    return 0;
}
