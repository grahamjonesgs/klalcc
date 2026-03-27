/* demo.c - Demonstrates C library functions on FPGA CPU
 *
 * Expected UART output:
 *   Hello FPGA!
 *   6 * 7 = 42
 *   factorial(10) = 3628800
 *   gcd(252, 105) = 21
 *   Primes up to 30:
 *   2 3 5 7 11 13 17 19 23 29
 *   Done.
 */

extern void putchar(int ch);
extern void print_str(int *s);
extern void print_int(int n);
extern void print_unsigned(unsigned n);
extern void print_hex(int val);
extern void newline(void);

int factorial(int n) {
    int r, i;
    r = 1;
    for (i = 2; i <= n; i = i + 1)
        r = r * i;
    return r;
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

int is_prime(int n) {
    int i;
    if (n <= 1) return 0;
    if (n <= 3) return 1;
    i = 2;
    while (i * i <= n) {
        if ((n % i) == 0) return 0;
        i = i + 1;
    }
    return 1;
}

/* String constants as word arrays */
int s_hello[]    = { 72,101,108,108,111,32,70,80,71,65,33, 0 };
int s_times[]    = { 32,42,32, 0 };       /* " * " */
int s_equals[]   = { 32,61,32, 0 };       /* " = " */
int s_fact[]     = { 102,97,99,116,111,114,105,97,108,40, 0 }; /* "factorial(" */
int s_rparen[]   = { 41, 0 };             /* ")" */
int s_gcd1[]     = { 103,99,100,40, 0 };  /* "gcd(" */
int s_comma[]    = { 44,32, 0 };           /* ", " */
int s_primes[]   = { 80,114,105,109,101,115,32,117,112,32,116,111,32,51,48,58, 0 };
                   /* "Primes up to 30:" */
int s_done[]     = { 68,111,110,101,46, 0 }; /* "Done." */
int s_space[]    = { 32, 0 };             /* " " */

int main() {
    int i;

    /* Hello FPGA! */
    print_str(s_hello);
    newline();

    /* 6 * 7 = 42 */
    print_int(6);
    print_str(s_times);
    print_int(7);
    print_str(s_equals);
    print_int(6 * 7);
    newline();

    /* factorial(10) = 3628800 */
    print_str(s_fact);
    print_int(10);
    print_str(s_rparen);
    print_str(s_equals);
    print_int(factorial(10));
    newline();

    /* gcd(252, 105) = 21 */
    print_str(s_gcd1);
    print_int(252);
    print_str(s_comma);
    print_int(105);
    print_str(s_rparen);
    print_str(s_equals);
    print_int(gcd(252, 105));
    newline();

    /* Primes up to 30 */
    print_str(s_primes);
    newline();
    for (i = 2; i <= 30; i = i + 1) {
        if (is_prime(i)) {
            print_int(i);
            putchar(32); /* space */
        }
    }
    newline();

    print_str(s_done);
    newline();
    return 0;
}
