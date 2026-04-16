/* test_types.c - type coercions, char operations, ternary, short-circuit,
 *                do-while, unsigned arithmetic
 *
 * Tests:
 *   T1  signed char promotion to int (sign extension at 0x80 boundary)
 *   T2  char masked to unsigned byte (zero-extension via & 0xFF)
 *   T3  int-to-char truncation (low 8 bits kept)
 *   T4  unsigned int wrapping and comparisons
 *   T5  mixed signed/unsigned comparisons
 *   T6  char arithmetic — ASCII case conversion
 *   T7  pointer-to-int cast and round-trip dereference
 *   T8  ternary operator (?:) in assignments and expressions
 *   T9  short-circuit evaluation of && and ||
 *   T10 do-while loop (body executes at least once)
 */

#include "lib/libc.h"

/* ----------------------------------------------------------------
 * Helper with a side effect: increments *counter and returns 1.
 * Used to detect whether an operand of && / || was evaluated.
 * ---------------------------------------------------------------- */
int side_effect(int *counter) {
    *counter = *counter + 1;
    return 1;
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
        print_hex_prefix(actual);
        print_str(" exp=");
        print_hex_prefix(expected);
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
    g_pass = 0;
    g_fail = 0;
    print_str("=== Type Coercion Test Suite ===");
    newline();

    /* ----------------------------------------------------------
     * T1: signed char promotion — sign extension
     *
     * char is 1 byte, CHAR_BIT=8.  When a signed char with bit 7 set
     * is promoted to int it should sign-extend to a negative int.
     *   0x7F (127) is the max positive; 0x7F+1 wraps to 0x80 = -128.
     *   0xFF stored as signed char = -1; promoted to int = -1.
     * ---------------------------------------------------------- */
    {
        char c;
        int  i;
        c = 0x7F;
        i = c;
        check_eq("T1a char 127:     ", i, 127);

        c = c + 1;      /* 127+1 truncated back to char = 0x80 = -128 signed */
        i = c;
        check_eq("T1b char -128:    ", i, -128);

        c = -1;         /* stored as 0xFF; promoted = -1 */
        i = c;
        check_eq("T1c char -1:      ", i, -1);

        c = 0;
        i = c;
        check_eq("T1d char 0:       ", i, 0);
    }

    /* ----------------------------------------------------------
     * T2: extract unsigned byte value via masking
     *
     * Char stored as 0xFF; masking with 0xFF gives 255, not -1.
     * ---------------------------------------------------------- */
    {
        char c;
        int  i;
        c = -1;             /* 0xFF in memory */
        i = c & 0xFF;
        check_eq("T2a mask -1==255: ", i, 255);

        c = -128;           /* 0x80 */
        i = c & 0xFF;
        check_eq("T2b mask -128==128:", i, 128);

        c = 1;
        i = c & 0xFF;
        check_eq("T2c mask 1==1:    ", i, 1);
    }

    /* ----------------------------------------------------------
     * T3: int-to-char truncation (keep low 8 bits)
     *
     * Assigning an int to a char discards all but the low byte.
     *   0x12345678 -> low byte 0x78 = 120 (positive, bit 7 clear)
     *   0x80 -> 0x80 = -128 as signed char
     * ---------------------------------------------------------- */
    {
        int  i;
        char c;
        i = 0x12345678;
        c = i;          /* keep 0x78 = 120 */
        check_eq("T3a trunc 0x78:   ", (int)c, 120);

        i = 0x180;      /* low byte = 0x80 = -128 as signed char */
        c = i;
        check_eq("T3b trunc 0x80:   ", (int)c, -128);

        i = 256;        /* 0x100: low byte = 0 */
        c = i;
        check_eq("T3c trunc 256==0: ", (int)c, 0);
    }

    /* ----------------------------------------------------------
     * T4: unsigned int wrapping
     *
     * On a 64-bit CPU unsigned is 64 bits.
     *   0 - 1 wraps to UINT64_MAX = (unsigned)(-1).
     *   0x80000000 is positive as unsigned (bit 63 is 0).
     * ---------------------------------------------------------- */
    {
        unsigned u;
        u = 0;
        u = u - 1;      /* wraps to all-ones = (unsigned)(-1) */
        check_eq("T4a u-1==UMAX:    ", (u == (unsigned)(-1)), 1);
        check("T4b UMAX>0:        ", u > 0);   /* unsigned: all-ones > 0 */

        u = 0x80000000;
        check("T4c 0x8000..>0:    ", u > 0);   /* unsigned: bit63=0, positive */
    }

    /* ----------------------------------------------------------
     * T5: mixed signed/unsigned comparisons
     *
     *   -1 as signed int is negative.
     *   (unsigned)(-1) is a very large positive number.
     * ---------------------------------------------------------- */
    {
        int      s;
        unsigned u;
        s = -1;
        u = 1;
        check_eq("T5a -1<1 signed:  ", (s < (int)u), 1);
        check_eq("T5b UMAX>1 u:     ", (int)((unsigned)s > u), 1);

        s = 0x7FFFFFFF;
        u = 0x80000000;
        /* signed: 0x7FFFFFFF < (int)0x80000000 because int cast makes it -2^31 */
        check_eq("T5c signed cmp:   ", (s > (int)u), 1);
        /* unsigned: 0x7FFFFFFF < 0x80000000 */
        check_eq("T5d unsigned cmp: ", (int)((unsigned)s < u), 1);
    }

    /* ----------------------------------------------------------
     * T6: char arithmetic — ASCII case conversion
     * ---------------------------------------------------------- */
    {
        char c;
        c = 'A';
        check_eq("T6a 'A'+32=='a':  ", (int)(c + 32), (int)'a');
        c = 'z';
        check_eq("T6b 'z'-32=='Z':  ", (int)(c - 32), (int)'Z');
        c = '0';
        check_eq("T6c '0'+5=='5':   ", (int)(c + 5),  (int)'5');
        c = '9';
        check_eq("T6d '9'-'0'==9:   ", (int)(c - '0'), 9);
    }

    /* ----------------------------------------------------------
     * T7: pointer-to-int cast and round-trip dereference
     * ---------------------------------------------------------- */
    {
        char buf[8];
        int  addr;
        buf[0] = 42;
        buf[1] = 99;
        addr = (int)buf;        /* address as integer */
        check_eq("T7a deref addr:   ", (int)(*(char *)addr),       42);
        check_eq("T7b deref addr+1: ", (int)(*(char *)(addr + 1)), 99);
    }

    /* ----------------------------------------------------------
     * T8: ternary operator (?:)
     * ---------------------------------------------------------- */
    {
        int x, y, z;
        x = 5;
        y = 10;

        z = (x > y) ? x : y;
        check_eq("T8a ternary max:  ", z, 10);

        z = (x < y) ? x : y;
        check_eq("T8b ternary min:  ", z, 5);

        z = (x == 5) ? 100 : 200;
        check_eq("T8c ternary true: ", z, 100);

        z = (x == 0) ? 100 : 200;
        check_eq("T8d ternary false:", z, 200);

        /* nested ternary: clamp x to [3, 7] */
        z = (x < 3) ? 3 : (x > 7) ? 7 : x;
        check_eq("T8e nested clamp: ", z, 5);
    }

    /* ----------------------------------------------------------
     * T9: short-circuit evaluation
     *
     * && evaluates right side only if left is non-zero.
     * || evaluates right side only if left is zero.
     * We detect evaluation via a side_effect() call that bumps a counter.
     * ---------------------------------------------------------- */
    {
        int count;

        /* T9a: 0 && side_effect() — right should NOT be called */
        count = 0;
        if (0 && side_effect(&count)) { }
        check_eq("T9a && skip:      ", count, 0);

        /* T9b: 1 || side_effect() — right should NOT be called */
        count = 0;
        if (1 || side_effect(&count)) { }
        check_eq("T9b || skip:      ", count, 0);

        /* T9c: 1 && side_effect() — right SHOULD be called */
        count = 0;
        if (1 && side_effect(&count)) { }
        check_eq("T9c && eval:      ", count, 1);

        /* T9d: 0 || side_effect() — right SHOULD be called */
        count = 0;
        if (0 || side_effect(&count)) { }
        check_eq("T9d || eval:      ", count, 1);

        /* T9e: chained &&: a && b && c; if a is false, b and c both skipped */
        count = 0;
        if (0 && side_effect(&count) && side_effect(&count)) { }
        check_eq("T9e && chain skip:", count, 0);
    }

    /* ----------------------------------------------------------
     * T10: do-while loop
     *
     * Body executes before the condition is tested, so it always
     * runs at least once — even when the condition is initially false.
     * ---------------------------------------------------------- */
    {
        int i, sum;

        /* standard do-while: sum 0+1+2+3+4+5 = 15 */
        i = 0;
        sum = 0;
        do {
            sum = sum + i;
            i = i + 1;
        } while (i <= 5);
        check_eq("T10a do-while 15: ", sum, 15);

        /* body runs once even when condition is false from the start */
        i = 100;
        sum = 0;
        do {
            sum = sum + 1;
        } while (i < 0);
        check_eq("T10b do-once:     ", sum, 1);
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
