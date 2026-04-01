%{
/*
 * lcc backend for FPGA_CPU_32_DDR_cache
 *
 * 32-bit word-addressed CPU, 16 GPRs (A-P), no FP hardware.
 * All C types are 32 bits (CHAR_BIT=32).
 *
 * Register convention:
 *   A-D  (0-3)   argument passing, caller-saved temps
 *   E-H  (4-7)   callee-saved, register variables
 *   I-L  (8-11)  caller-saved temps
 *   M    (12)    function return value
 *   N    (13)    scratch for codegen (not allocatable)
 *   O    (14)    caller-saved temp
 *   P    (15)    frame pointer (FP)
 *
 * Stack: hardware DDR2 stack (PUSH/POP/GETSP/SETSP/ADDSP).
 *        SP is not in a GPR; accessed only via stack opcodes.
 */

#define INTTMP 0x00004F0F  /* A-D(0-3), I-L(8-11), O(14) as temps */
#define INTVAR 0x000000F0  /* E-H(4-7) as register variables */
#define INTRET 0x00001000  /* M(12) return value */

#include "c.h"
#define NODEPTR_TYPE Node
#define OP_LABEL(p) ((p)->op)
#define LEFT_CHILD(p) ((p)->kids[0])
#define RIGHT_CHILD(p) ((p)->kids[1])
#define STATE_LABEL(p) ((p)->x.state)

static void address(Symbol, Symbol, long);
static void blkfetch(int, int, int, int);
static void blkloop(int, int, int, int, int, int[]);
static void blkstore(int, int, int, int);
static void defaddress(Symbol);
static void defconst(int, int, Value);
static void defstring(int, char *);
static void defsymbol(Symbol);
static void doarg(Node);
static void emit2(Node);
static void export(Symbol);
static void clobber(Node);
static void function(Symbol, Symbol [], Symbol [], int);
static void global(Symbol);
static void import(Symbol);
static void local(Symbol);
static void progbeg(int, char **);
static void progend(void);
static void segment(int);
static void space(int);
static void target(Node);

static Symbol ireg[32];
static Symbol iregw;
static Symbol blkreg;
static int cseg;
static int framesize_actual;
static char *curfunc = "?";

/* Format an immediate value for .kla output. */
static char *imm(int v) {
    return stringf("%d", v);
}

/* Register indices */
enum {
    REG_A=0, REG_B, REG_C, REG_D,
    REG_E, REG_F, REG_G, REG_H,
    REG_I, REG_J, REG_K, REG_L,
    REG_M, REG_N, REG_O, REG_P
};

%}

%start stmt

%term CNSTF1=1041
%term CNSTI1=1045
%term CNSTI2=2069
%term CNSTP1=1047
%term CNSTU1=1046
%term CNSTU2=2070

%term ARGB=41
%term ARGF1=1057
%term ARGI1=1061
%term ARGP1=1063
%term ARGU1=1062

%term ASGNB=57
%term ASGNF1=1073
%term ASGNI1=1077
%term ASGNI2=2101
%term ASGNP1=1079
%term ASGNU1=1078
%term ASGNU2=2102

%term INDIRB=73
%term INDIRF1=1089
%term INDIRI1=1093
%term INDIRI2=2117
%term INDIRP1=1095
%term INDIRU1=1094
%term INDIRU2=2118

%term CVFF1=1137
%term CVFI1=1141
%term CVIF1=1153
%term CVII1=1157
%term CVII2=2181
%term CVIU1=1158
%term CVIU2=2182
%term CVPP1=1175
%term CVPU1=1174
%term CVUI1=1205
%term CVUI2=2229
%term CVUP1=1207
%term CVUU1=1206
%term CVUU2=2230

%term NEGF1=1217
%term NEGI1=1221

%term CALLB=217
%term CALLF1=1233
%term CALLI1=1237
%term CALLP1=1239
%term CALLU1=1238
%term CALLV=216

%term RETF1=1265
%term RETI1=1269
%term RETP1=1271
%term RETU1=1270
%term RETV=248

%term ADDRGP1=1287
%term ADDRFP1=1303
%term ADDRLP1=1319

%term ADDF1=1329
%term ADDI1=1333
%term ADDP1=1335
%term ADDU1=1334

%term SUBF1=1345
%term SUBI1=1349
%term SUBP1=1351
%term SUBU1=1350

%term LSHI1=1365
%term LSHU1=1366

%term MODI1=1381
%term MODU1=1382

%term RSHI1=1397
%term RSHU1=1398

%term BANDI1=1413
%term BANDU1=1414

%term BCOMI1=1429
%term BCOMU1=1430

%term BORI1=1445
%term BORU1=1446

%term BXORI1=1461
%term BXORU1=1462

%term DIVF1=1473
%term DIVI1=1477
%term DIVU1=1478

%term MULF1=1489
%term MULI1=1493
%term MULU1=1494

%term EQF1=1505
%term EQI1=1509
%term EQU1=1510

%term GEF1=1521
%term GEI1=1525
%term GEU1=1526

%term GTF1=1537
%term GTI1=1541
%term GTU1=1542

%term LEF1=1553
%term LEI1=1557
%term LEU1=1558

%term LTF1=1569
%term LTI1=1573
%term LTU1=1574

%term NEF1=1585
%term NEI1=1589
%term NEU1=1590

%term JUMPV=584
%term LABELV=600

%term LOADB=233
%term LOADI1=1253
%term LOADI2=2277
%term LOADP1=1255
%term LOADU1=1254
%term LOADU2=2278

%term VREGP=711
%%

reg: INDIRI1(VREGP)     "# read register\n"
reg: INDIRU1(VREGP)     "# read register\n"
reg: INDIRP1(VREGP)     "# read register\n"

stmt: ASGNI1(VREGP,reg)  "# write register\n"
stmt: ASGNU1(VREGP,reg)  "# write register\n"
stmt: ASGNP1(VREGP,reg)  "# write register\n"


reg: CNSTI1  "SETR %c %a\n"  1
reg: CNSTU1  "SETR %c %a\n"  1
reg: CNSTP1  "SETR %c %a\n"  1

acon: CNSTI1   "%a"
acon: CNSTU1   "%a"
acon: CNSTP1   "%a"
acon: ADDRGP1  "%a"

reg: acon  "SETR %c %0\n"  1

reg: ADDRFP1  "# addr FP\n"  2
reg: ADDRLP1  "# addr LP\n"  2

reg: INDIRI1(reg)     "MEMREADRR %c %0\n"  1
reg: INDIRU1(reg)     "MEMREADRR %c %0\n"  1
reg: INDIRP1(reg)     "MEMREADRR %c %0\n"  1

stmt: ASGNI1(reg,reg)  "MEMSETRR %1 %0\n"  1
stmt: ASGNU1(reg,reg)  "MEMSETRR %1 %0\n"  1
stmt: ASGNP1(reg,reg)  "MEMSETRR %1 %0\n"  1

reg: ADDI1(reg,reg)    "# add\n"  2
reg: ADDU1(reg,reg)    "# add\n"  2
reg: ADDP1(reg,reg)    "# add\n"  2

reg: SUBI1(reg,reg)    "# sub\n"  2
reg: SUBU1(reg,reg)    "# sub\n"  2
reg: SUBP1(reg,reg)    "# sub\n"  2

reg: MULI1(reg,reg)    "# mul\n"  3
reg: MULU1(reg,reg)    "# mul\n"  3

reg: DIVI1(reg,reg)    "# div\n"  35
reg: DIVU1(reg,reg)    "# div\n"  35

reg: MODI1(reg,reg)    "# mod\n"  35
reg: MODU1(reg,reg)    "# mod\n"  35

reg: NEGI1(reg)        "# neg\n"  2

reg: BANDI1(reg,reg)   "# and\n"  2
reg: BANDU1(reg,reg)   "# and\n"  2
reg: BORI1(reg,reg)    "# or\n"   2
reg: BORU1(reg,reg)    "# or\n"   2
reg: BXORI1(reg,reg)   "# xor\n"  2
reg: BXORU1(reg,reg)   "# xor\n"  2
reg: BCOMI1(reg)       "# not\n" 1
reg: BCOMU1(reg)       "# not\n" 1

rc5: CNSTI1  "%a"  range(a, 0, 31)
rc5: CNSTU1  "%a"  range(a, 0, 31)

reg: LSHI1(reg,rc5)    "COPY %c %0\nSHLV %c %1\n"  2
reg: LSHU1(reg,rc5)    "COPY %c %0\nSHLV %c %1\n"  2
reg: LSHI1(reg,reg)    "# lsh_var\n"  2
reg: LSHU1(reg,reg)    "# lsh_var\n"  2
reg: RSHI1(reg,rc5)    "COPY %c %0\nSHRAV %c %1\n"  2
reg: RSHU1(reg,rc5)    "COPY %c %0\nSHRV %c %1\n"   2
reg: RSHI1(reg,reg)    "# rsha_var\n" 2
reg: RSHU1(reg,reg)    "# rshl_var\n" 2

reg: LOADI1(reg)  "COPY %c %0\n"  move(a)
reg: LOADU1(reg)  "COPY %c %0\n"  move(a)
reg: LOADI2(reg)  "COPY %c %0\n"  move(a)
reg: LOADU2(reg)  "COPY %c %0\n"  move(a)
reg: LOADI1(reg)  "COPY %c %0\n"  move(a)
reg: LOADP1(reg)  "COPY %c %0\n"  move(a)
reg: LOADU1(reg)  "COPY %c %0\n"  move(a)

reg: CVII1(reg)  "COPY %c %0\n"  move(a)
reg: CVUI1(reg)  "COPY %c %0\n"  move(a)
reg: CVIU1(reg)  "COPY %c %0\n"  move(a)
reg: CVUU1(reg)  "COPY %c %0\n"  move(a)
reg: CVPU1(reg)  "COPY %c %0\n"  move(a)
reg: CVUP1(reg)  "COPY %c %0\n"  move(a)
reg: CVPP1(reg)  "COPY %c %0\n"  move(a)

stmt: EQI1(reg,reg)   "CMPRR %0 %1\nJMPE %a:\n"    2
stmt: EQU1(reg,reg)   "CMPRR %0 %1\nJMPE %a:\n"    2
stmt: NEI1(reg,reg)   "CMPRR %0 %1\nJMPNE %a:\n"   2
stmt: NEU1(reg,reg)   "CMPRR %0 %1\nJMPNE %a:\n"   2

stmt: LTI1(reg,reg)   "CMPRR %0 %1\nJMPLT %a:\n"   2
stmt: LEI1(reg,reg)   "CMPRR %0 %1\nJMPLE %a:\n"   2
stmt: GTI1(reg,reg)   "CMPRR %0 %1\nJMPGT %a:\n"   2
stmt: GEI1(reg,reg)   "CMPRR %0 %1\nJMPGE %a:\n"   2

stmt: LTU1(reg,reg)   "CMPRR %0 %1\nJMPULT %a:\n"   2
stmt: LEU1(reg,reg)   "CMPRR %0 %1\nJMPULE %a:\n"    2
stmt: GTU1(reg,reg)   "CMPRR %0 %1\nJMPUGT %a:\n"    2
stmt: GEU1(reg,reg)   "CMPRR %0 %1\nJMPUGE %a:\n"    2

stmt: LABELV           "%a:\n"
stmt: JUMPV(acon)      "JMP %0:\n"     1
stmt: JUMPV(reg)       "JMPR %0\n"    1

reg:  CALLI1(acon)      "CALL %0:\n"    1
reg:  CALLU1(acon)      "CALL %0:\n"    1
reg:  CALLP1(acon)      "CALL %0:\n"    1
stmt: CALLV(acon)       "CALL %0:\n"    1

reg:  CALLI1(reg)       "CALLR %0\n"    2
reg:  CALLU1(reg)       "CALLR %0\n"    2
reg:  CALLP1(reg)       "CALLR %0\n"    2
stmt: CALLV(reg)        "CALLR %0\n"    2

stmt: RETI1(reg)       "# ret\n"      1
stmt: RETU1(reg)       "# ret\n"      1
stmt: RETP1(reg)       "# ret\n"      1
stmt: RETV(reg)        "# ret\n"      1

stmt: ARGI1(reg)       "# arg\n"      1
stmt: ARGU1(reg)       "# arg\n"      1
stmt: ARGP1(reg)       "# arg\n"      1

stmt: ARGB(INDIRB(reg))       "# argb %0\n"      1
stmt: ASGNB(reg,INDIRB(reg))  "# asgnb %0 %1\n"  1

stmt: reg  ""

%%
/* ========================================================================
 * Interface function implementations
 * ======================================================================== */

static void progbeg(int argc, char *argv[]) {
    int i;
    static char *rnames[] = {"A","B","C","D","E","F","G","H",
                              "I","J","K","L","M","N","O","P"};

    {
        union { char c; int i; } u;
        u.i = 0; u.c = 1;
        swap = ((int)(u.i == 1)) != IR->little_endian;
    }

    parseflags(argc, argv);

    /* Create 16 integer registers: A(0) through P(15) */
    for (i = 0; i < 16; i++)
        ireg[i] = mkreg(rnames[i], i, 1, IREG);

    iregw = mkwildcard(ireg);

    /* Temp registers: A-D(0-3), I-M(8-12), O(14) */
    tmask[IREG] = INTTMP | INTRET;
    tmask[FREG] = 0;

    /* Register variables: E-H(4-7) */
    vmask[IREG] = INTVAR;
    vmask[FREG] = 0;

    /* Block copy scratch register */
    blkreg = mkreg("N", REG_N, 7, IREG);
}

static void progend(void) {
    /* nothing */
}

static Symbol rmap(int opk) {
    switch (optype(opk)) {
    case I: case U: case P: case B:
        return iregw;
    default:
        return 0;
    }
}

/*
 * emit2 - called for each instruction to handle special templates
 *
 * Templates starting with "# " are pseudo-instructions handled here.
 * This avoids encoding complex multi-instruction sequences in templates.
 */
static void emit2(Node p) {
    int dst = -1;
    int op = specific(p->op);

    /* Only get dest register for nodes that produce a value (have a result) */
    if (p->syms[RX] && p->syms[RX]->x.regnode)
        dst = getregnum(p);

    switch (op) {

    /* --- Address computation for frame-relative access --- */
    case ADDRF+P: {
        /* Parameter address: FP + off (off is always negative) */
        int off = p->syms[0]->x.offset;
        print("COPY %s P\nMINUSV %s %s\n",
            ireg[dst]->x.name, ireg[dst]->x.name,
            imm(-off));
        break;
    }
    case ADDRL+P: {
        /* Local variable address: FP + off (off is always negative) */
        int off = p->syms[0]->x.offset;
        print("COPY %s P\nMINUSV %s %s\n",
            ireg[dst]->x.name, ireg[dst]->x.name,
            imm(-off));
        break;
    }

    /* --- Arithmetic --- */
    case ADD+I: case ADD+U: case ADD+P: {
        int src0 = getregnum(p->kids[0]);
        int src1 = getregnum(p->kids[1]);
        print("ADDR %s %s %s\n", ireg[dst]->x.name, ireg[src0]->x.name, ireg[src1]->x.name);
        break;
    }
    case SUB+I: case SUB+U: case SUB+P: {
        int src0 = getregnum(p->kids[0]);
        int src1 = getregnum(p->kids[1]);
        print("SUBR %s %s %s\n", ireg[dst]->x.name, ireg[src0]->x.name, ireg[src1]->x.name);
        break;
    }
    case MUL+I: {
        int src0 = getregnum(p->kids[0]);
        int src1 = getregnum(p->kids[1]);
        print("MULR %s %s %s\n", ireg[dst]->x.name, ireg[src0]->x.name, ireg[src1]->x.name);
        break;
    }
    case MUL+U: {
        int src0 = getregnum(p->kids[0]);
        int src1 = getregnum(p->kids[1]);
        print("MULUR %s %s %s\n", ireg[dst]->x.name, ireg[src0]->x.name, ireg[src1]->x.name);
        break;
    }
    case DIV+I: {
        int src0 = getregnum(p->kids[0]);
        int src1 = getregnum(p->kids[1]);
        print("DIVR %s %s %s\n", ireg[dst]->x.name, ireg[src0]->x.name, ireg[src1]->x.name);
        break;
    }
    case DIV+U: {
        int src0 = getregnum(p->kids[0]);
        int src1 = getregnum(p->kids[1]);
        print("DIVUR %s %s %s\n", ireg[dst]->x.name, ireg[src0]->x.name, ireg[src1]->x.name);
        break;
    }
    case MOD+I: {
        int src0 = getregnum(p->kids[0]);
        int src1 = getregnum(p->kids[1]);
        print("MODR %s %s %s\n", ireg[dst]->x.name, ireg[src0]->x.name, ireg[src1]->x.name);
        break;
    }
    case MOD+U: {
        int src0 = getregnum(p->kids[0]);
        int src1 = getregnum(p->kids[1]);
        print("MODUR %s %s %s\n", ireg[dst]->x.name, ireg[src0]->x.name, ireg[src1]->x.name);
        break;
    }
    case NEG+I: {
        int src0 = getregnum(p->kids[0]);
        if (dst != src0)
            print("COPY %s %s\n", ireg[dst]->x.name, ireg[src0]->x.name);
        print("NEGR %s\n", ireg[dst]->x.name);
        break;
    }

    /* --- Bitwise --- */
    case BAND+I: case BAND+U: {
        int src0 = getregnum(p->kids[0]);
        int src1 = getregnum(p->kids[1]);
        print("ANDR %s %s %s\n", ireg[dst]->x.name, ireg[src0]->x.name, ireg[src1]->x.name);
        break;
    }
    case BOR+I: case BOR+U: {
        int src0 = getregnum(p->kids[0]);
        int src1 = getregnum(p->kids[1]);
        print("ORR %s %s %s\n", ireg[dst]->x.name, ireg[src0]->x.name, ireg[src1]->x.name);
        break;
    }
    case BXOR+I: case BXOR+U: {
        int src0 = getregnum(p->kids[0]);
        int src1 = getregnum(p->kids[1]);
        print("XORR %s %s %s\n", ireg[dst]->x.name, ireg[src0]->x.name, ireg[src1]->x.name);
        break;
    }
    case BCOM+I: case BCOM+U: {
        int src0 = getregnum(p->kids[0]);
        if (dst != src0)
            print("COPY %s %s\n", ireg[dst]->x.name, ireg[src0]->x.name);
        print("XORV %s 0xFFFFFFFF\n", ireg[dst]->x.name);
        break;
    }

    /* --- Shifts --- */
    case LSH+I: case LSH+U: {
        int src0 = getregnum(p->kids[0]);
        int src1 = getregnum(p->kids[1]);
        print("SHLR %s %s %s\n", ireg[dst]->x.name, ireg[src0]->x.name, ireg[src1]->x.name);
        break;
    }
    case RSH+I: {
        int src0 = getregnum(p->kids[0]);
        int src1 = getregnum(p->kids[1]);
        print("SARR %s %s %s\n", ireg[dst]->x.name, ireg[src0]->x.name, ireg[src1]->x.name);
        break;
    }
    case RSH+U: {
        int src0 = getregnum(p->kids[0]);
        int src1 = getregnum(p->kids[1]);
        print("SHRR %s %s %s\n", ireg[dst]->x.name, ireg[src0]->x.name, ireg[src1]->x.name);
        break;
    }

    /* --- Function arguments --- */
    case ARG+I: case ARG+U: case ARG+P: {
        int argno = p->x.argno;
        if (argno >= 4) {
            /* Stack argument: store at offset from hardware SP */
            int src = getregnum(p->x.kids[0]);
            int stkoff = p->syms[2]->u.c.v.i; /* already in words */
            print("GETSP N\n");
            print("STIDX %s N %d\n", ireg[src]->x.name, stkoff);
        }
        break;
    }

    /* --- Block (struct) copy --- */
    case ASGN+B: {
        static int tmpregs[] = {REG_N, REG_I, REG_J};
        dalign = salign = p->syms[1]->u.c.v.i;
        blkcopy(getregnum(p->x.kids[0]), 0,
                getregnum(p->x.kids[1]), 0,
                p->syms[0]->u.c.v.i, tmpregs);
        break;
    }

    default:
        break;
    }
}

static void target(Node p) {
    assert(p);
    switch (specific(p->op)) {
    case CALL+V:
        break;
    case CALL+I: case CALL+U: case CALL+P:
        /* Return value comes back in M (R12) */
        setreg(p, ireg[REG_M]);
        break;
    case RET+I: case RET+U: case RET+P:
        /* Move return value to M (R12) */
        rtarget(p, 0, ireg[REG_M]);
        break;
    case ARG+I: case ARG+U: case ARG+P: {
        int argno = p->x.argno;
        if (argno < 4) {
            /* First 4 args go in A-D */
            rtarget(p, 0, ireg[argno]);
        }
        break;
    }
    }
}

static void clobber(Node p) {
    assert(p);
    switch (specific(p->op)) {
    case CALL+I: case CALL+U: case CALL+P:
        /* Don't spill M - it holds the return value */
        spill(INTTMP, IREG, p);
        break;
    case CALL+V:
        /* Void call clobbers everything including M */
        spill(INTTMP | INTRET, IREG, p);
        break;
    }
}

static void doarg(Node p) {
    static int argno;

    if (argoffset == 0)
        argno = 0;
    p->x.argno = argno++;
    /* With size=1, each arg is 1 word. Align to 1. */
    p->syms[2] = intconst(mkactual(1, p->syms[0]->u.c.v.i));
}

static void local(Symbol p) {
    if (askregvar(p, rmap(ttob(p->type))) == 0)
        mkauto(p);
}

static void function(Symbol f, Symbol caller[], Symbol callee[], int ncalls) {
    int i, saved, varargs, numparams;

    usedmask[0] = usedmask[1] = 0;
    freemask[0] = freemask[1] = ~(unsigned)0;
    offset = maxoffset = maxargoffset = 0;

    /* Count params */
    for (i = 0; callee[i]; i++)
        ;
    numparams = i;
    varargs = variadic(f->type)
        || numparams > 0 && strcmp(callee[numparams-1]->name, "va_alist") == 0;

    /* Process parameters — each is 1 word.
     * Offsets are negated (like mkauto) so all frame access uses FP + off
     * where off is always negative: param 0 at FP-1, param 1 at FP-2, etc.
     */
    for (i = 0; callee[i]; i++) {
        Symbol p = callee[i];
        Symbol q = caller[i];
        assert(q);
        offset = roundup(offset + q->type->size, q->type->align);
        p->x.offset = q->x.offset = -offset;
        p->x.name = q->x.name = stringd(-offset);

        if (i < 4 && !varargs
            && ncalls == 0 && !isstruct(q->type)
            && !p->addressed) {
            /* Keep in register */
            p->sclass = q->sclass = REGISTER;
            askregvar(p, ireg[i]);
            assert(p->x.regnode && p->x.regnode->vbl == p);
            q->x = p->x;
            q->type = p->type;
        } else if (askregvar(p, rmap(ttob(p->type)))
                   && i < 4
                   && (isint(p->type) || p->type == q->type)) {
            p->sclass = q->sclass = REGISTER;
            q->type = p->type;
        }
    }
    assert(!caller[i]);

    /* Parameters that are not kept in registers will be saved as locals */
    for (i = 0; callee[i]; i++) {
        Symbol p = callee[i];
        if (p->sclass != REGISTER) {
            p->sclass = AUTO;
        }
    }

    /* Generate code — offset is NOT reset so locals start after param slots */
    gencode(caller, callee);

    /* Ensure maxoffset covers all param frame slots */
    if (maxoffset < numparams)
        maxoffset = numparams;

    /* Count callee-saved registers used */
    usedmask[IREG] &= INTVAR; /* only care about callee-saved regs */
    saved = 0;
    for (i = REG_E; i <= REG_H; i++)
        if (usedmask[IREG] & (1 << i))
            saved++;

    /* Frame size in words: locals/spills + saved regs + outgoing args */
    framesize_actual = maxargoffset + maxoffset + saved;

    /* === Emit prologue === */
    curfunc = f->x.name;
    segment(CODE);
    print("%s:\n", f->x.name);

    /* Save old FP and set up new frame using hardware stack */
    print("PUSH P\n");              /* save old FP on DDR2 stack */
    print("GETSP P\n");             /* FP = SP */

    /* Allocate frame */
    if (framesize_actual > 0)
        print("ADDSP %s\n", imm(-framesize_actual));

    /* Save callee-saved registers (below locals in frame) */
    {
        int slot = 0;
        for (i = REG_E; i <= REG_H; i++) {
            if (usedmask[IREG] & (1 << i)) {
                print("STIDX %s P %s\n", ireg[i]->x.name,
                    imm(-(maxoffset + slot + 1)));
                slot++;
            }
        }
    }

    /* Save register arguments (0-3) to frame if needed.
     * Store when callee is AUTO (body reads from frame),
     * or when caller/callee differ (gencode inserts a transfer
     * that loads from the frame slot).
     */
    for (i = 0; i < 4 && callee[i]; i++) {
        Symbol p = callee[i];
        Symbol q = caller[i];
        if (p->sclass != REGISTER
            || p->sclass != q->sclass
            || p->type != q->type) {
            print("// save arg %d\n", i);
            print("STIDX %s P %s\n", ireg[i]->x.name,
                imm(p->x.offset));
            if (p->sclass != REGISTER)
                p->sclass = q->sclass = AUTO;
        }
    }

    /* Move register args to their callee-saved register variables.
     * When askregvar assigned a param to a callee-saved reg (E-H),
     * the value is still in the argument reg (A-D).  Generate COPY.
     */
    for (i = 0; i < 4 && callee[i]; i++) {
        Symbol p = callee[i];
        if (p->sclass == REGISTER && p->x.regnode
            && p->x.regnode->number != i) {
            print("COPY %s %s\n", ireg[p->x.regnode->number]->x.name,
                ireg[i]->x.name);
        }
    }

    /* Copy overflow arguments (>=4) from caller's stack to local frame.
     * After CALL (pushes ret addr) + PUSH P (saves old FP) + GETSP P:
     *   caller_SP = FP + 2
     *   overflow arg with stkoff is at FP + stkoff + 2
     * For arg index i, stkoff = i (since each arg is 1 word).
     */
    for (i = 4; i < numparams; i++) {
        Symbol p = callee[i];
        if (p->sclass != REGISTER) {
            print("// copy overflow arg %d\n", i);
            print("LDIDX N P %d\n", i + 2);
            print("STIDX N P %s\n", imm(p->x.offset));
        }
    }

    /* === Emit body === */
    emitcode();

    /* === Emit epilogue === */
    {
        int slot = 0;
        for (i = REG_E; i <= REG_H; i++) {
            if (usedmask[IREG] & (1 << i)) {
                print("LDIDX %s P %s\n", ireg[i]->x.name,
                    imm(-(maxoffset + slot + 1)));
                slot++;
            }
        }
    }

    /* Restore SP and FP using hardware stack ops */
    print("SETSP P\n");             /* SP = FP (discard locals) */
    print("POP P\n");               /* restore old FP */
    print("RET\n");
}

/* === Data emission === */

static void defconst(int suffix, int size, Value v) {
    switch (suffix) {
    case I:
        print(".word 0x%x\n", (unsigned)(v.i));
        break;
    case U:
        print(".word 0x%x\n", (unsigned)(v.u));
        break;
    case P:
        print(".word 0x%x\n", (unsigned)(unsigned long)(v.p));
        break;
    case F:
        /* No FP hardware - emit raw bits */
        {
            float fv = v.d;
            print(".word 0x%x\n", *(unsigned *)&fv);
        }
        break;
    default:
        assert(0);
    }
}

static void defaddress(Symbol p) {
    print(".word %s\n", p->x.name);
}

static void defstring(int n, char *str) {
    char *s;
    /* Each character occupies one 32-bit word (CHAR_BIT=32) */
    for (s = str; s < str + n; s++)
        print(".word 0x%x\n", (*s) & 0xFF);
}

static void export(Symbol p) {
    /* .kla format: no export directive needed */
}

static void import(Symbol p) {
    /* .kla format: no import directive needed */
}

static void defsymbol(Symbol p) {
    if (p->scope >= LOCAL && p->sclass == STATIC)
        p->x.name = stringf("%s_L_%d", cfunc ? cfunc->name : "G", genlabel(1));
    else if (p->generated)
        p->x.name = stringf("%s_L_%s", cfunc ? cfunc->name : "G", p->name);
    else
        assert(p->scope != CONSTANTS || isint(p->type) || isptr(p->type)),
        p->x.name = p->name;
}

static void address(Symbol q, Symbol p, long n) {
    if (p->scope == GLOBAL
        || p->sclass == STATIC || p->sclass == EXTERN)
        q->x.name = stringf("%s%s%D", p->x.name,
            n >= 0 ? "+" : "", n);
    else {
        assert(n <= INT_MAX && n >= INT_MIN);
        q->x.offset = p->x.offset + n;
        q->x.name = stringd(q->x.offset);
    }
}

static void global(Symbol p) {
    if (p->u.seg == BSS) {
        /* Emit label so SETR can reference it, then allocate space */
        print("%s:\n", p->x.name);
        print("#%s %d\n", p->x.name, p->type->size);
    } else {
        print("%s:\n", p->x.name);
    }
}

static void segment(int n) {
    cseg = n;
    /* .kla format: no segment directives needed */
}

static void space(int n) {
    if (cseg != BSS)
        print(".space %d\n", n);  /* n is in words (size=1) */
}

/* === Block copy support === */

static void blkfetch(int size, int off, int reg, int tmp) {
    assert(size == 1);
    print("LDIDX %s %s %d\n", ireg[tmp]->x.name, ireg[reg]->x.name, off);
}

static void blkstore(int size, int off, int reg, int tmp) {
    assert(size == 1);
    print("STIDX %s %s %d\n", ireg[tmp]->x.name, ireg[reg]->x.name, off);
}

static void blkloop(int dreg, int doff, int sreg, int soff, int size, int tmps[]) {
    int lab = genlabel(1);
    print("SETR %s %d\n", ireg[tmps[2]]->x.name, size);
    print("%s_SH_%d:\n", curfunc, lab);
    blkcopy(dreg, doff, sreg, soff, 1, tmps);
    print("INCR %s\n", ireg[sreg]->x.name);
    print("INCR %s\n", ireg[dreg]->x.name);
    print("DECR %s\n", ireg[tmps[2]]->x.name);
    print("CMPRV %s 0\n", ireg[tmps[2]]->x.name);
    print("JMPNZ %s_SH_%d:\n", curfunc, lab);
}

/* === Stab stubs (no debug info for now) === */

static char rcsid[] = "$Id: klacpu.md$";

/* === THE INTERFACE RECORD === */

Interface klacpuIR = {
    1, 1, 0,  /* char:     size=1, align=1 (one word per addressable unit) */
    1, 1, 0,  /* short:    size=1, align=1 */
    1, 1, 0,  /* int:      size=1, align=1 */
    1, 1, 0,  /* long:     size=1, align=1 */
    1, 1, 0,  /* longlong: size=1, align=1 */
    1, 1, 1,  /* float:    size=1, align=1, outofline=1 (no FP hw) */
    1, 1, 1,  /* double:   size=1, align=1, outofline=1 */
    1, 1, 1,  /* longdouble: same */
    1, 1, 0,  /* T*:       size=1, align=1 */
    0, 1, 0,  /* struct:   size=0, align=1 */
    0,        /* little_endian = 0 (big-endian) */
    0,        /* mulops_calls = 0 (hardware mul/div for ints) */
    0,        /* wants_callb = 0 */
    1,        /* wants_argb = 1 */
    1,        /* left_to_right = 1 */
    0,        /* wants_dag = 0 */
    0,        /* unsigned_char = 0 */
    address,
    blockbeg,
    blockend,
    defaddress,
    defconst,
    defstring,
    defsymbol,
    emit,
    export,
    function,
    gen,
    global,
    import,
    local,
    progbeg,
    progend,
    segment,
    space,
    0, 0, 0, 0, 0, 0, 0,  /* no stab/debug */
    {
        1,      /* max_unaligned_load = 1 word */
        rmap,
        blkfetch, blkstore, blkloop,
        _label,
        _rule,
        _nts,
        _kids,
        _string,
        _templates,
        _isinstruction,
        _ntname,
        emit2,
        doarg,
        target,
        clobber,
    }
};
