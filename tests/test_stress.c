/* test_struct.c - structs and 5+ argument functions */

/* 5 args - 5th goes on stack */
int add5(int a, int b, int c, int d, int e) {
    return a + b + c + d + e;
}

/* Nested expressions */
int expr(int a, int b, int c) {
    return (a * b + c) * (a - b) - c;
}

/* Multiple local variables with complex control */
int is_prime(int n) {
    int i;
    if (n <= 1) return 0;
    if (n <= 3) return 1;
    if ((n % 2) == 0) return 0;
    i = 3;
    while (i * i <= n) {
        if ((n % i) == 0)
            return 0;
        i = i + 2;
    }
    return 1;
}

/* Chained function calls */
int double_val(int x) { return x + x; }
int quad(int x) { return double_val(double_val(x)); }
