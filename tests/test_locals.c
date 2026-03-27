/* test_locals.c - local variables, for loops, arrays */
int factorial(int n) {
    int result;
    int i;
    result = 1;
    for (i = 2; i <= n; i = i + 1)
        result = result * i;
    return result;
}

int fib(int n) {
    int a, b, t, i;
    if (n <= 1) return n;
    a = 0;
    b = 1;
    for (i = 2; i <= n; i = i + 1) {
        t = a + b;
        a = b;
        b = t;
    }
    return b;
}

int array_sum(int *arr, int n) {
    int s, i;
    s = 0;
    for (i = 0; i < n; i = i + 1)
        s = s + *(arr + i);
    return s;
}
