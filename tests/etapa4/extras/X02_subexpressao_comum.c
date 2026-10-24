int calcula(int a, int b) {
    int x;
    int y;
    x = a * b + 1;
    y = a * b + 2;
    return x + y;
}

int main() {
    print(calcula(3, 7));
    return 0;
}
