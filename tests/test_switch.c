/* test_switch.c - switch statements and complex control */
int classify(int x) {
    if (x < 0) return -1;
    if (x == 0) return 0;
    return 1;
}

int collatz_steps(int n) {
    int steps;
    steps = 0;
    while (n != 1) {
        if (n & 1)
            n = n * 3 + 1;
        else
            n = n >> 1;
        steps = steps + 1;
    }
    return steps;
}

int gcd(int a, int b) {
    while (b != 0) {
        int t;
        t = b;
        b = a % b;
        a = t;
    }
    return a;
}
