/* test_sort.c - sorting algorithms and binary search
 *
 * Tests:
 *   T1  bubble sort: unsorted 8-element array
 *   T2  insertion sort: different unsorted 8-element array
 *   T3  binary search: found, not-found, edge cases
 *   T4  already-sorted input (degenerate best-case for bubble sort)
 *   T5  reverse-sorted input (worst-case for insertion sort)
 *   T6  single-element array
 *   T7  duplicate elements
 */

#include "lib/libc.h"

/* ----------------------------------------------------------------
 * is_sorted: returns 1 if arr[0..n-1] is non-decreasing
 * ---------------------------------------------------------------- */
int is_sorted(int *arr, int n) {
    int i;
    for (i = 0; i < n - 1; i = i + 1) {
        if (arr[i] > arr[i + 1]) return 0;
    }
    return 1;
}

/* ----------------------------------------------------------------
 * bubble_sort: O(n^2), in-place, stable
 * ---------------------------------------------------------------- */
void bubble_sort(int *arr, int n) {
    int i, j, tmp;
    for (i = 0; i < n - 1; i = i + 1) {
        for (j = 0; j < n - 1 - i; j = j + 1) {
            if (arr[j] > arr[j + 1]) {
                tmp        = arr[j];
                arr[j]     = arr[j + 1];
                arr[j + 1] = tmp;
            }
        }
    }
}

/* ----------------------------------------------------------------
 * insertion_sort: O(n^2), in-place, stable
 * ---------------------------------------------------------------- */
void insertion_sort(int *arr, int n) {
    int i, j, key;
    for (i = 1; i < n; i = i + 1) {
        key = arr[i];
        j   = i - 1;
        while (j >= 0 && arr[j] > key) {
            arr[j + 1] = arr[j];
            j = j - 1;
        }
        arr[j + 1] = key;
    }
}

/* ----------------------------------------------------------------
 * binary_search: sorted arr[0..n-1]; returns index or -1
 * ---------------------------------------------------------------- */
int binary_search(int *arr, int n, int target) {
    int lo, hi, mid;
    lo = 0;
    hi = n - 1;
    while (lo <= hi) {
        mid = lo + (hi - lo) / 2;
        if (arr[mid] == target) return mid;
        if (arr[mid] < target)  lo = mid + 1;
        else                    hi = mid - 1;
    }
    return -1;
}

/* ----------------------------------------------------------------
 * copy_arr: copy src[0..n-1] to dst
 * ---------------------------------------------------------------- */
void copy_arr(int *dst, int *src, int n) {
    int i;
    for (i = 0; i < n; i = i + 1)
        dst[i] = src[i];
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
    int src[8];
    int arr[8];
    int i;

    g_pass = 0;
    g_fail = 0;
    print_str("=== Sort Test Suite ===");
    newline();

    /* Canonical unsorted input: {5,2,8,1,9,3,7,4} */
    src[0] = 5;  src[1] = 2;  src[2] = 8;  src[3] = 1;
    src[4] = 9;  src[5] = 3;  src[6] = 7;  src[7] = 4;

    /* ----------------------------------------------------------
     * T1: bubble sort on unsorted 8-element array
     * ---------------------------------------------------------- */
    copy_arr(arr, src, 8);
    bubble_sort(arr, 8);
    check("T1a sorted:       ", is_sorted(arr, 8));
    check_eq("T1b arr[0]==1:    ", arr[0], 1);
    check_eq("T1c arr[7]==9:    ", arr[7], 9);
    check_eq("T1d arr[4]==5:    ", arr[4], 5);

    /* ----------------------------------------------------------
     * T2: insertion sort on a different unsorted array
     * ---------------------------------------------------------- */
    arr[0] = 10; arr[1] = 3;  arr[2] = 15; arr[3] = 7;
    arr[4] = 1;  arr[5] = 12; arr[6] = 6;  arr[7] = 20;
    insertion_sort(arr, 8);
    check("T2a sorted:       ", is_sorted(arr, 8));
    check_eq("T2b arr[0]==1:    ", arr[0], 1);
    check_eq("T2c arr[7]==20:   ", arr[7], 20);
    check_eq("T2d arr[3]==7:    ", arr[3], 7);

    /* ----------------------------------------------------------
     * T3: binary search on sorted result from T1 (1..9)
     * ---------------------------------------------------------- */
    copy_arr(arr, src, 8);
    bubble_sort(arr, 8);                    /* arr = {1,2,3,4,5,6,7,8,9} -- wait, 8 elems */
    check_eq("T3a find 5:       ", binary_search(arr, 8, 5), 4);
    check_eq("T3b find 1:       ", binary_search(arr, 8, 1), 0);
    check_eq("T3c find 9:       ", binary_search(arr, 8, 9), 7);
    check_eq("T3d not found:    ", binary_search(arr, 8, 99), -1);
    check_eq("T3e not found 0:  ", binary_search(arr, 8, 0), -1);

    /* ----------------------------------------------------------
     * T4: already-sorted input (best case)
     * ---------------------------------------------------------- */
    for (i = 0; i < 8; i = i + 1)
        arr[i] = i + 1;
    bubble_sort(arr, 8);
    check("T4  already sort: ", is_sorted(arr, 8));
    check_eq("T4b arr[0]==1:    ", arr[0], 1);

    /* ----------------------------------------------------------
     * T5: reverse-sorted input (worst case for insertion sort)
     * ---------------------------------------------------------- */
    for (i = 0; i < 8; i = i + 1)
        arr[i] = 8 - i;    /* {8,7,6,5,4,3,2,1} */
    insertion_sort(arr, 8);
    check("T5a rev sort:     ", is_sorted(arr, 8));
    check_eq("T5b arr[0]==1:    ", arr[0], 1);
    check_eq("T5c arr[7]==8:    ", arr[7], 8);

    /* ----------------------------------------------------------
     * T6: single-element array — both sorts must leave it intact
     * ---------------------------------------------------------- */
    arr[0] = 42;
    bubble_sort(arr, 1);
    check_eq("T6a bubble 1:     ", arr[0], 42);
    arr[0] = 99;
    insertion_sort(arr, 1);
    check_eq("T6b ins 1:        ", arr[0], 99);

    /* ----------------------------------------------------------
     * T7: duplicate elements
     * ---------------------------------------------------------- */
    arr[0] = 3; arr[1] = 1; arr[2] = 3; arr[3] = 2;
    arr[4] = 1; arr[5] = 2; arr[6] = 3; arr[7] = 1;
    bubble_sort(arr, 8);
    check("T7a dup sorted:   ", is_sorted(arr, 8));
    check_eq("T7b arr[0]==1:    ", arr[0], 1);
    check_eq("T7c arr[7]==3:    ", arr[7], 3);

    /* binary search on array with duplicates — any matching index is valid */
    {
        int idx;
        arr[0] = 1; arr[1] = 2; arr[2] = 2; arr[3] = 3;
        arr[4] = 3; arr[5] = 3; arr[6] = 4; arr[7] = 5;
        idx = binary_search(arr, 8, 3);
        check("T7d dup bsearch:  ", idx >= 3 && idx <= 5);
        check_eq("T7e arr[idx]==3:  ", arr[idx], 3);
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
