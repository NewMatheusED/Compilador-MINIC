int main() {
    int x = 1;
    int i = 0;
    while (i < 3) {
        int x = i * 10;
        int j = 0;
        while (j < 2) {
            int x = 100;
            j = j + 1;
        }
        print(x);
        i = i + 1;
    }
    print(x);
    return 0;
}
