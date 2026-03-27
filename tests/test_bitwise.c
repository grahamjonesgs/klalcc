/* test_bitwise.c - bitwise operations */
int band(int a, int b) { return a & b; }
int bor(int a, int b) { return a | b; }
int bxor(int a, int b) { return a ^ b; }
int bnot(int a) { return ~a; }
int shl_c(int a) { return a << 4; }
int shr_c(int a) { return a >> 3; }
unsigned shr_u(unsigned a) { return a >> 5; }
