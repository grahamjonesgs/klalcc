/* test_struct.c - structs, struct pointers, nested structs, arrays of structs
 *
 * Tests:
 *   T1  basic member assignment and read (dot notation)
 *   T2  struct pointer access via ->
 *   T3  modify struct fields through pointer
 *   T4  squared-distance computation (two struct pointers)
 *   T5  nested struct access (struct Rect contains two struct Points)
 *   T6  array of structs: sequential assign and read
 *   T7  pointer arithmetic over an array of structs
 *   T8  heap-allocated struct via malloc/free
 */

#include "lib/libc.h"

struct Point {
    int x;
    int y;
};

/* sizeof(Point) = 2 * sizeof(int) = 16 bytes */

struct Rect {
    struct Point min;
    struct Point max;
    int id;
};

/* sizeof(Rect) = 2*sizeof(Point) + sizeof(int) = 40 bytes */

/* ----------------------------------------------------------------
 * Helper functions
 * ---------------------------------------------------------------- */
int point_sum(struct Point *p) {
    return p->x + p->y;
}

void point_scale(struct Point *p, int factor) {
    p->x = p->x * factor;
    p->y = p->y * factor;
}

int point_dist_sq(struct Point *a, struct Point *b) {
    int dx, dy;
    dx = a->x - b->x;
    dy = a->y - b->y;
    return dx*dx + dy*dy;
}

int rect_area(struct Rect *r) {
    int w, h;
    w = r->max.x - r->min.x;
    h = r->max.y - r->min.y;
    return w * h;
}

/* ----------------------------------------------------------------
 * Test infrastructure
 * ---------------------------------------------------------------- */
int g_pass;
int g_fail;

void check_eq(char *name, int actual, int expected) {
    print_str(name);
    if (actual == expected) {
        print_str("PASS");
        g_pass = g_pass + 1;
    } else {
        print_str("FAIL  got=");
        print_int(actual);
        print_str(" exp=");
        print_int(expected);
        g_fail = g_fail + 1;
    }
    newline();
}

void check(char *name, int ok) {
    print_str(name);
    if (ok) {
        print_str("PASS");
        g_pass = g_pass + 1;
    } else {
        print_str("FAIL");
        g_fail = g_fail + 1;
    }
    newline();
}

/* ================================================================
 * main
 * ================================================================ */
int main(void) {
    struct Point p1;
    struct Point p2;
    struct Rect  r;
    struct Point arr[4];
    int i;

    g_pass = 0;
    g_fail = 0;
    print_str("=== Struct Test Suite ===");
    newline();

    /* ----------------------------------------------------------
     * T1: basic struct member assignment and read (dot notation)
     * ---------------------------------------------------------- */
    p1.x = 3;
    p1.y = 4;
    check_eq("T1a p1.x==3:      ", p1.x, 3);
    check_eq("T1b p1.y==4:      ", p1.y, 4);

    /* ----------------------------------------------------------
     * T2: pass struct pointer, read fields via ->
     * ---------------------------------------------------------- */
    check_eq("T2  ptr->x+y==7:  ", point_sum(&p1), 7);

    /* ----------------------------------------------------------
     * T3: modify struct fields through a pointer
     * ---------------------------------------------------------- */
    point_scale(&p1, 2);
    check_eq("T3a scaled x==6:  ", p1.x, 6);
    check_eq("T3b scaled y==8:  ", p1.y, 8);

    /* ----------------------------------------------------------
     * T4: two struct pointers, squared distance
     * (p1=(6,8), p2=(0,0)) => dist_sq = 36+64 = 100
     * ---------------------------------------------------------- */
    p2.x = 0;
    p2.y = 0;
    check_eq("T4  dist_sq==100: ", point_dist_sq(&p1, &p2), 100);

    /* ----------------------------------------------------------
     * T5: nested struct — struct Rect contains two struct Points
     * ---------------------------------------------------------- */
    r.min.x = 1;
    r.min.y = 2;
    r.max.x = 6;
    r.max.y = 5;
    r.id    = 42;
    check_eq("T5a area==15:     ", rect_area(&r), 15);
    check_eq("T5b id==42:       ", r.id, 42);
    check_eq("T5c min.x==1:     ", r.min.x, 1);
    check_eq("T5d max.y==5:     ", r.max.y, 5);

    /* ----------------------------------------------------------
     * T6: array of structs — assign and read sequentially
     * ---------------------------------------------------------- */
    for (i = 0; i < 4; i = i + 1) {
        arr[i].x = i * 10;
        arr[i].y = i * 10 + 1;
    }
    check_eq("T6a arr[0].x==0:  ", arr[0].x, 0);
    check_eq("T6b arr[0].y==1:  ", arr[0].y, 1);
    check_eq("T6c arr[1].x==10: ", arr[1].x, 10);
    check_eq("T6d arr[3].x==30: ", arr[3].x, 30);
    check_eq("T6e arr[3].y==31: ", arr[3].y, 31);

    /* ----------------------------------------------------------
     * T7: pointer arithmetic over an array of structs
     * sizeof(struct Point)=16 so pp+1 advances 16 bytes
     * ---------------------------------------------------------- */
    {
        struct Point *pp;
        int dist;
        pp = arr;
        check_eq("T7a *pp x==0:     ", pp->x, 0);
        pp = pp + 1;
        check_eq("T7b pp+1 x==10:   ", pp->x, 10);
        pp = pp + 1;
        check_eq("T7c pp+2 x==20:   ", pp->x, 20);
        pp = pp + 1;
        check_eq("T7d pp+3 x==30:   ", pp->x, 30);
        /* byte distance: 3 * 16 = 48 */
        dist = (int)((char *)(&arr[3]) - (char *)(&arr[0]));
        check_eq("T7e dist==48:     ", dist, 48);
    }

    /* ----------------------------------------------------------
     * T8: heap-allocated struct via malloc/free
     * sizeof(struct Point) = 16 bytes
     * ---------------------------------------------------------- */
    {
        struct Point *hp;
        hp = (struct Point *)malloc(16);
        hp->x = 100;
        hp->y = 200;
        check_eq("T8a heap->x==100: ", hp->x, 100);
        check_eq("T8b heap->y==200: ", hp->y, 200);
        point_scale(hp, 3);
        check_eq("T8c scaled x==300:", hp->x, 300);
        free(hp);
        check_eq("T8d heap clean:   ", heap_words_used(), 0);
    }

    /* ---------------------------------------------------------- */
    newline();
    print_str("Results: ");
    print_int(g_pass);
    print_str(" pass, ");
    print_int(g_fail);
    print_str(" fail");
    newline();
    return g_fail;
}
