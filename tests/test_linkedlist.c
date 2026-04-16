/* test_linkedlist.c - singly-linked list using malloc + structs
 *
 * Integration test that exercises: struct definitions, struct pointer
 * manipulation (->), malloc/free, pointer traversal, and null-pointer
 * sentinel values all together.
 *
 * Tests:
 *   T1  push three values, check length, sum, and head->val
 *   T2  list_contains: found and not-found cases
 *   T3  list_reverse: head changes, length and sum preserved
 *   T4  list_free: heap_words_used() returns to 0
 *   T5  empty-list edge cases (null head)
 *   T6  larger list (10 elements, 1..10, sum=55)
 *   T7  list_nth: index-based access
 *   T8  list_remove_head: pop from front
 */

#include "lib/libc.h"

/* ----------------------------------------------------------------
 * Node: val(8 bytes) + next pointer(8 bytes) = 16 bytes total.
 * malloc(16) allocates exactly one node's worth of data.
 * ---------------------------------------------------------------- */
struct Node {
    int         val;
    struct Node *next;
};

/* ----------------------------------------------------------------
 * list_push: prepend val; returns new head.
 * ---------------------------------------------------------------- */
struct Node *list_push(struct Node *head, int val) {
    struct Node *node;
    node = (struct Node *)malloc(16);
    node->val  = val;
    node->next = head;
    return node;
}

/* ----------------------------------------------------------------
 * list_length: count nodes.
 * ---------------------------------------------------------------- */
int list_length(struct Node *head) {
    int n;
    n = 0;
    while (head != 0) {
        n    = n + 1;
        head = head->next;
    }
    return n;
}

/* ----------------------------------------------------------------
 * list_sum: sum all val fields.
 * ---------------------------------------------------------------- */
int list_sum(struct Node *head) {
    int s;
    s = 0;
    while (head != 0) {
        s    = s + head->val;
        head = head->next;
    }
    return s;
}

/* ----------------------------------------------------------------
 * list_contains: 1 if val found, 0 otherwise.
 * ---------------------------------------------------------------- */
int list_contains(struct Node *head, int val) {
    while (head != 0) {
        if (head->val == val) return 1;
        head = head->next;
    }
    return 0;
}

/* ----------------------------------------------------------------
 * list_reverse: reverse in-place; returns new head.
 * ---------------------------------------------------------------- */
struct Node *list_reverse(struct Node *head) {
    struct Node *prev;
    struct Node *curr;
    struct Node *next;
    prev = 0;
    curr = head;
    while (curr != 0) {
        next       = curr->next;
        curr->next = prev;
        prev       = curr;
        curr       = next;
    }
    return prev;
}

/* ----------------------------------------------------------------
 * list_free: free all nodes.
 * ---------------------------------------------------------------- */
void list_free(struct Node *head) {
    struct Node *next;
    while (head != 0) {
        next = head->next;
        free(head);
        head = next;
    }
}

/* ----------------------------------------------------------------
 * list_nth: return val at zero-based index, or -1 if out of range.
 * ---------------------------------------------------------------- */
int list_nth(struct Node *head, int idx) {
    while (head != 0) {
        if (idx == 0) return head->val;
        idx  = idx - 1;
        head = head->next;
    }
    return -1;
}

/* ----------------------------------------------------------------
 * list_remove_head: free the head node; returns new head.
 * ---------------------------------------------------------------- */
struct Node *list_remove_head(struct Node *head) {
    struct Node *next;
    if (head == 0) return 0;
    next = head->next;
    free(head);
    return next;
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
    struct Node *list;
    struct Node *rev;

    g_pass = 0;
    g_fail = 0;
    print_str("=== Linked List Test Suite ===");
    newline();

    /* ----------------------------------------------------------
     * T1: push 1, 2, 3 (head-insert => list is [3,2,1])
     * ---------------------------------------------------------- */
    list = 0;
    list = list_push(list, 1);
    list = list_push(list, 2);
    list = list_push(list, 3);
    check_eq("T1a len==3:       ", list_length(list), 3);
    check_eq("T1b sum==6:       ", list_sum(list), 6);
    check_eq("T1c head==3:      ", list->val, 3);
    check_eq("T1d head->next==2:", list->next->val, 2);

    /* ----------------------------------------------------------
     * T2: list_contains
     * ---------------------------------------------------------- */
    check("T2a has 1:        ", list_contains(list, 1));
    check("T2b has 2:        ", list_contains(list, 2));
    check("T2c has 3:        ", list_contains(list, 3));
    check_eq("T2d no 99:        ", list_contains(list, 99), 0);
    check_eq("T2e no 0:         ", list_contains(list, 0), 0);

    /* ----------------------------------------------------------
     * T3: list_reverse => list becomes [1,2,3]
     * ---------------------------------------------------------- */
    rev = list_reverse(list);
    /* list pointer now dangling — do not use it after reverse */
    check_eq("T3a rev head==1:  ", rev->val, 1);
    check_eq("T3b rev len==3:   ", list_length(rev), 3);
    check_eq("T3c rev sum==6:   ", list_sum(rev), 6);
    check_eq("T3d rev[1]==2:    ", list_nth(rev, 1), 2);
    check_eq("T3e rev[2]==3:    ", list_nth(rev, 2), 3);

    /* ----------------------------------------------------------
     * T4: list_free — heap must return to empty
     * ---------------------------------------------------------- */
    list_free(rev);
    check_eq("T4  heap clean:   ", heap_words_used(), 0);

    /* ----------------------------------------------------------
     * T5: empty-list edge cases
     * ---------------------------------------------------------- */
    check_eq("T5a empty len:    ", list_length(0), 0);
    check_eq("T5b empty sum:    ", list_sum(0),    0);
    check_eq("T5c empty nth:    ", list_nth(0, 0), -1);
    check_eq("T5d remove null:  ", (int)list_remove_head(0), 0);

    /* ----------------------------------------------------------
     * T6: larger list: 1..10 (sum = 55)
     * ---------------------------------------------------------- */
    {
        int i;
        list = 0;
        for (i = 1; i <= 10; i = i + 1)
            list = list_push(list, i);
        check_eq("T6a len==10:      ", list_length(list), 10);
        check_eq("T6b sum==55:      ", list_sum(list),    55);
        check_eq("T6c head==10:     ", list->val,         10);
        check_eq("T6d nth(9)==1:    ", list_nth(list, 9), 1);
        list_free(list);
        check_eq("T6e heap clean:   ", heap_words_used(), 0);
    }

    /* ----------------------------------------------------------
     * T7: list_nth index-based access
     * ---------------------------------------------------------- */
    {
        list = 0;
        list = list_push(list, 30);
        list = list_push(list, 20);
        list = list_push(list, 10);
        /* list = [10, 20, 30] */
        check_eq("T7a nth(0)==10:   ", list_nth(list, 0), 10);
        check_eq("T7b nth(1)==20:   ", list_nth(list, 1), 20);
        check_eq("T7c nth(2)==30:   ", list_nth(list, 2), 30);
        check_eq("T7d nth(3)==-1:   ", list_nth(list, 3), -1);
        list_free(list);
    }

    /* ----------------------------------------------------------
     * T8: list_remove_head (pop from front)
     * ---------------------------------------------------------- */
    {
        list = 0;
        list = list_push(list, 3);
        list = list_push(list, 2);
        list = list_push(list, 1);
        /* list = [1, 2, 3] */
        list = list_remove_head(list);
        check_eq("T8a after pop==2: ", list->val,         2);
        check_eq("T8b len==2:       ", list_length(list), 2);
        list = list_remove_head(list);
        check_eq("T8c after pop==3: ", list->val,         3);
        list = list_remove_head(list);
        check_eq("T8d list empty:   ", (int)(list == 0),  1);
        check_eq("T8e heap clean:   ", heap_words_used(), 0);
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
