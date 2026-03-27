/* test_data.c - global data, initialized globals, string access */
int table[4] = { 10, 20, 30, 40 };
int count = 0;

int lookup(int idx) {
    return table[idx];
}

void inc_count(void) {
    count = count + 1;
}

int get_count(void) {
    return count;
}

/* String as array of word-sized chars */
int msg[] = { 72, 101, 108, 108, 111, 0 }; /* "Hello" */

int strlen_w(int *s) {
    int len;
    len = 0;
    while (*s != 0) {
        len = len + 1;
        s = s + 1;
    }
    return len;
}
