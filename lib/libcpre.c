#line 1 "libc.c"

#line 8 "libc.c"


extern void _uart_tx_hex(int val);
extern void _uart_newline(void);
extern void _uart_tx_char(int ch);
extern int  _uart_rx_char(void);
extern int  _uart_rx_char_nb(void);


void print_unsigned(unsigned n);
void _print_neg(int neg, int orig);
void print_str(int *s);


#line 24 "libc.c"

void putchar(int ch) {
    _uart_tx_char(ch);
}


int getchar(void) {
    return _uart_rx_char();
}


#line 37 "libc.c"
int getchar_nb(void) {
    return _uart_rx_char_nb();
}

void puts(int *s) {
    while (*s != 0) {
        putchar(*s);
        s = s + 1;
    }
    putchar(10);
}


void print_str(int *s) {
    while (*s != 0) {
        putchar(*s);
        s = s + 1;
    }
}


#line 60 "libc.c"


void print_unsigned(unsigned n) {

    if (n >= 10)
        print_unsigned(n / 10);
    putchar((n % 10) + 48);
}


void print_int(int n) {
    if (n < 0) {
        putchar(45);

        _print_neg(-n, n);
        return;
    }
    print_unsigned(n);
}


#line 82 "libc.c"
void _print_neg(int neg, int orig) {
    if (neg < 0) {

        putchar(50); putchar(49); putchar(52); putchar(55);
        putchar(52); putchar(56); putchar(51); putchar(54);
        putchar(52); putchar(56);
    } else {
        print_unsigned(neg);
    }
}


void print_hex(int val) {
    _uart_tx_hex(val);
}


void print_hex_prefix(int val) {
    putchar(48);
    putchar(120);
    _uart_tx_hex(val);
}


void newline(void) {
    _uart_newline();
}


#line 115 "libc.c"


#line 119 "libc.c"


int strlen(int *s) {
    int len;
    len = 0;
    while (*s != 0) {
        len = len + 1;
        s = s + 1;
    }
    return len;
}


int strcmp(int *a, int *b) {
    while (*a != 0 && *a == *b) {
        a = a + 1;
        b = b + 1;
    }
    return *a - *b;
}


int *strcpy(int *dst, int *src) {
    int *ret;
    ret = dst;
    while (*src != 0) {
        *dst = *src;
        dst = dst + 1;
        src = src + 1;
    }
    *dst = 0;
    return ret;
}


#line 156 "libc.c"


void *memset(int *dst, int c, int n) {
    int *p;
    p = dst;
    while (n > 0) {
        *p = c;
        p = p + 1;
        n = n - 1;
    }
    return dst;
}


void *memcpy(int *dst, int *src, int n) {
    int *d;
    int *s;
    d = dst;
    s = src;
    while (n > 0) {
        *d = *s;
        d = d + 1;
        s = s + 1;
        n = n - 1;
    }
    return dst;
}


#line 187 "libc.c"


int abs(int n) {
    if (n < 0) return -n;
    return n;
}


int min(int a, int b) {
    if (a < b) return a;
    return b;
}

int max(int a, int b) {
    if (a > b) return a;
    return b;
}


void swap(int *a, int *b) {
    int t;
    t = *a;
    *a = *b;
    *b = t;
}


#line 241 "libc.c"




static int  _heap_inited = 0;
static int *_free_head   = 0;


#line 250 "libc.c"
static void _heap_init(void) {
    int *mem;
    mem          = (int *)0;
    mem[1]       = mem[0];
    _free_head   = 0;
    _heap_inited = 1;
}


#line 261 "libc.c"
void *malloc(int size) {
    int *prev;
    int *curr;
    int *split;
    int *blk;
    int *mem;
    int  total;

    if (!_heap_inited) _heap_init();
    if (size <= 0) return 0;


    prev = 0;
    curr = _free_head;
    while (curr != 0) {
        if (curr[0] >= size) {

            if (curr[0] >= size + 3 + 1) {

                split    = curr + 3 + size;
                split[0] = curr[0] - size - 3;
                split[1] = 1;
                split[2] = curr[2];
                if (prev != 0)
                    prev[2] = (int)split;
                else
                    _free_head = split;
                curr[0] = size;
            } else {

                if (prev != 0)
                    prev[2] = curr[2];
                else
                    _free_head = (int *)curr[2];
            }
            curr[1] = 0;
            curr[2] = 0;
            return (void *)(curr + 3);
        }
        prev = curr;
        curr = (int *)curr[2];
    }


    mem   = (int *)0;
    total = 3 + size;
    blk   = (int *)mem[1];
    mem[1] = (int)(blk + total);

    blk[0] = size;
    blk[1] = 0;
    blk[2] = 0;
    return (void *)(blk + 3);
}


#line 319 "libc.c"
void free(void *ptr) {
    int *blk;
    int *prev;
    int *curr;
    int *adj;

    if (ptr == 0) return;

    blk      = (int *)ptr - 3;
    blk[1]   = 1;


    prev = 0;
    curr = _free_head;
    while (curr != 0 && curr < blk) {
        prev = curr;
        curr = (int *)curr[2];
    }
    blk[2] = (int)curr;
    if (prev != 0)
        prev[2] = (int)blk;
    else
        _free_head = blk;


    if (curr != 0) {
        adj = blk + 3 + blk[0];
        if (adj == curr) {
            blk[0] = blk[0] + 3 + curr[0];
            blk[2] = curr[2];

        }
    }


    if (prev != 0) {
        adj = prev + 3 + prev[0];
        if (adj == blk) {
            prev[0] = prev[0] + 3 + blk[0];
            prev[2] = blk[2];
        }
    }
}


#line 365 "libc.c"
void *calloc(int nmemb, int size) {
    int   total;
    void *ptr;

    if (nmemb <= 0 || size <= 0) return 0;
    total = nmemb * size;
    ptr   = malloc(total);
    if (ptr != 0)
        memset((int *)ptr, 0, total);
    return ptr;
}


#line 382 "libc.c"
void *realloc(void *ptr, int new_size) {
    int  *blk;
    int   old_size;
    void *new_ptr;

    if (ptr == 0)       return malloc(new_size);
    if (new_size <= 0)  { free(ptr); return 0; }

    blk      = (int *)ptr - 3;
    old_size = blk[0];
    if (new_size <= old_size) return ptr;

    new_ptr = malloc(new_size);
    if (new_ptr == 0) return 0;
    memcpy((int *)new_ptr, (int *)ptr, old_size);
    free(ptr);
    return new_ptr;
}


#line 403 "libc.c"
int heap_words_used(void) {
    int *mem;
    int *blk;
    int  used;

    if (!_heap_inited) return 0;
    mem  = (int *)0;
    blk  = (int *)mem[0];
    used = 0;
    while (blk < (int *)mem[1]) {
        if (blk[1] == 0)
            used = used + blk[0];
        blk = blk + 3 + blk[0];
    }
    return used;
}


int heap_words_free(void) {
    int *curr;
    int  free_words;

    free_words = 0;
    curr       = _free_head;
    while (curr != 0) {
        free_words = free_words + curr[0];
        curr = (int *)curr[2];
    }
    return free_words;
}
