/* test_control.c - control flow */
int max(int a, int b) {
    if (a > b)
        return a;
    return b;
}

int abs_val(int a) {
    if (a < 0)
        return -a;
    return a;
}

int sum_to_n(int n) {
    int s;
    int i;
    s = 0;
    i = 1;
    while (i <= n) {
        s = s + i;
        i = i + 1;
    }
    return s;
}
