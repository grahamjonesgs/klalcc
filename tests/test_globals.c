/* test_globals.c - global variables and pointers */
int counter;

void increment(void) {
    counter = counter + 1;
}

int get_counter(void) {
    return counter;
}

void set_via_ptr(int *p, int val) {
    *p = val;
}

int get_via_ptr(int *p) {
    return *p;
}
