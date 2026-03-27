/* test_recursive.c - recursive functions */
int factorial_r(int n) {
    if (n <= 1) return 1;
    return n * factorial_r(n - 1);
}

int fib_r(int n) {
    if (n <= 0) return 0;
    if (n == 1) return 1;
    return fib_r(n - 1) + fib_r(n - 2);
}

int power(int base, int exp) {
    if (exp == 0) return 1;
    return base * power(base, exp - 1);
}
