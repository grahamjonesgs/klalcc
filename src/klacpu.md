%{
/*
 * lcc backend for FPGA_CPU_64_DDR_cache
 *
 * Byte-addressed CPU (CHAR_BIT=8). All registers are 64-bit.
 * sizeof(char)=1, sizeof(short)=sizeof(int)=sizeof(long)=sizeof(T*)=4.
 * Each spill/frame slot is 8 bytes (one 64-bit register).
 *
 * Memory instructions:
 *   MEMGET8  dst src  — load 1 byte (zero-extended) from byte address in src
 *   MEMSET8  src dst  — store low byte of src to byte address in dst
 *   MEMGET64 dst src  — load 8-byte word from byte address in src into dst
 *   MEMSET64 src dst  — store 8-byte word to byte address in dst
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
 *        SP is not in a GPR; ADDSP takes byte count; STIDX/LDIDX take byte offsets.
 *
 * Frame layout (byte offsets from FP, FP set after PUSH P / GETSP P):
 *   [FP+0]        saved old FP (PUSH P)
 *   [FP+4]        return address (CALL)
 *   [FP+8+i*4]    overflow arg i (i >= 4, caller placed on stack)
 *   [FP-4]..[FP-maxoffset]    locals and spills
 *   [FP-(maxoffset+4)]..[FP-(maxoffset+saved*4)]  callee-saved regs
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

%term CNSTI4=4117
%term CNSTP4=4119
%term CNSTU4=4118

%term ARGI4=4133
%term ARGP4=4135
%term ARGU4=4134

%term ASGNI4=4149
%term ASGNP4=4151
%term ASGNU4=4150

%term INDIRI4=4165
%term INDIRP4=4167
%term INDIRU4=4166

%term CVII4=4229
%term CVIU4=4230
%term CVPP4=4247
%term CVPU4=4246
%term CVUI4=4277
%term CVUP4=4279
%term CVUU4=4278

%term NEGI4=4293

%term CALLI4=4309
%term CALLP4=4311
%term CALLU4=4310

%term RETI4=4341
%term RETP4=4343
%term RETU4=4342

%term ADDRGP4=4359
%term ADDRFP4=4375
%term ADDRLP4=4391

%term ADDI4=4405
%term ADDP4=4407
%term ADDU4=4406

%term SUBI4=4421
%term SUBP4=4423
%term SUBU4=4422

%term LSHI4=4437
%term LSHU4=4438

%term MODI4=4453
%term MODU4=4454

%term RSHI4=4469
%term RSHU4=4470

%term BANDI4=4485
%term BANDU4=4486

%term BCOMI4=4501
%term BCOMU4=4502

%term BORI4=4517
%term BORU4=4518

%term BXORI4=4533
%term BXORU4=4534

%term DIVI4=4549
%term DIVU4=4550

%term MULI4=4565
%term MULU4=4566

%term EQI4=4581
%term EQU4=4582

%term GEI4=4597
%term GEU4=4598

%term GTI4=4613
%term GTU4=4614

%term LEI4=4629
%term LEU4=4630

%term LTI4=4645
%term LTU4=4646

%term NEI4=4661
%term NEU4=4662

%term LOADI4=4325
%term LOADP4=4327
%term LOADU4=4326

/* === Size-8 terms (sizeof(int)=sizeof(void*)=8) === */
%term CNSTI8=8213
%term CNSTP8=8215
%term CNSTU8=8214

%term ARGI8=8229
%term ARGP8=8231
%term ARGU8=8230

%term ASGNI8=8245
%term ASGNP8=8247
%term ASGNU8=8246

%term INDIRI8=8261
%term INDIRP8=8263
%term INDIRU8=8262

%term CVII8=8325
%term CVIU8=8326
%term CVPP8=8343
%term CVPU8=8342
%term CVUI8=8373
%term CVUP8=8375
%term CVUU8=8374

%term NEGI8=8389

%term CALLI8=8405
%term CALLP8=8407
%term CALLU8=8406

%term RETI8=8437
%term RETP8=8439
%term RETU8=8438

%term ADDRGP8=8455
%term ADDRFP8=8471
%term ADDRLP8=8487

%term ADDI8=8501
%term ADDP8=8503
%term ADDU8=8502

%term SUBI8=8517
%term SUBP8=8519
%term SUBU8=8518

%term LSHI8=8533
%term LSHU8=8534

%term MODI8=8549
%term MODU8=8550

%term RSHI8=8565
%term RSHU8=8566

%term BANDI8=8581
%term BANDU8=8582

%term BCOMI8=8597
%term BCOMU8=8598

%term BORI8=8613
%term BORU8=8614

%term BXORI8=8629
%term BXORU8=8630

%term DIVI8=8645
%term DIVU8=8646

%term MULI8=8661
%term MULU8=8662

%term EQI8=8677
%term EQU8=8678

%term GEI8=8693
%term GEU8=8694

%term GTI8=8709
%term GTU8=8710

%term LEI8=8725
%term LEU8=8726

%term LTI8=8741
%term LTU8=8742

%term NEI8=8757
%term NEU8=8758

%term LOADI8=8421
%term LOADP8=8423
%term LOADU8=8422

%%

reg: INDIRI1(VREGP)     "# read register\n"
reg: INDIRU1(VREGP)     "# read register\n"
reg: INDIRP1(VREGP)     "# read register\n"
reg: INDIRI4(VREGP)     "# read register\n"
reg: INDIRU4(VREGP)     "# read register\n"
reg: INDIRP4(VREGP)     "# read register\n"

stmt: ASGNI1(VREGP,reg)  "# write register\n"
stmt: ASGNU1(VREGP,reg)  "# write register\n"
stmt: ASGNP1(VREGP,reg)  "# write register\n"
stmt: ASGNI4(VREGP,reg)  "# write register\n"
stmt: ASGNU4(VREGP,reg)  "# write register\n"
stmt: ASGNP4(VREGP,reg)  "# write register\n"

reg: CNSTI1  "SETR %c %a\n"  1
reg: CNSTU1  "SETR %c %a\n"  1
reg: CNSTP1  "SETR %c %a\n"  1
reg: CNSTI4  "SETR %c %a\n"  1
reg: CNSTU4  "SETR %c %a\n"  1
reg: CNSTP4  "SETR %c %a\n"  1

acon: CNSTI1   "%a"
acon: CNSTU1   "%a"
acon: CNSTP1   "%a"
acon: CNSTI4   "%a"
acon: CNSTU4   "%a"
acon: CNSTP4   "%a"
acon: ADDRGP1  "%a"
acon: ADDRGP4  "%a"

reg: acon  "SETR %c %0\n"  1

reg: ADDRFP1  "# addr FP\n"  2
reg: ADDRLP1  "# addr LP\n"  2
reg: ADDRFP4  "# addr FP\n"  2
reg: ADDRLP4  "# addr LP\n"  2

reg: INDIRI1(reg)     "MEMGET8 %c %0\n"   1
reg: INDIRU1(reg)     "MEMGET8 %c %0\n"   1

reg: INDIRI4(reg)     "MEMGET64 %c %0\n"   1
reg: INDIRU4(reg)     "MEMGET64 %c %0\n"   1
reg: INDIRP4(reg)     "MEMGET64 %c %0\n"   1

stmt: ASGNI1(reg,reg)  "MEMSET8 %1 %0\n"   1
stmt: ASGNU1(reg,reg)  "MEMSET8 %1 %0\n"   1

stmt: ASGNI4(reg,reg)  "MEMSET64 %1 %0\n"  1
stmt: ASGNU4(reg,reg)  "MEMSET64 %1 %0\n"  1
stmt: ASGNP4(reg,reg)  "MEMSET64 %1 %0\n"  1

reg: ADDI1(reg,reg)    "# add\n"  2
reg: ADDU1(reg,reg)    "# add\n"  2
reg: ADDP1(reg,reg)    "# add\n"  2
reg: ADDI4(reg,reg)    "# add\n"  2
reg: ADDU4(reg,reg)    "# add\n"  2
reg: ADDP4(reg,reg)    "# add\n"  2

reg: SUBI1(reg,reg)    "# sub\n"  2
reg: SUBU1(reg,reg)    "# sub\n"  2
reg: SUBP1(reg,reg)    "# sub\n"  2
reg: SUBI4(reg,reg)    "# sub\n"  2
reg: SUBU4(reg,reg)    "# sub\n"  2
reg: SUBP4(reg,reg)    "# sub\n"  2

reg: MULI1(reg,reg)    "# mul\n"  3
reg: MULU1(reg,reg)    "# mul\n"  3
reg: MULI4(reg,reg)    "# mul\n"  3
reg: MULU4(reg,reg)    "# mul\n"  3

reg: DIVI1(reg,reg)    "# div\n"  35
reg: DIVU1(reg,reg)    "# div\n"  35
reg: DIVI4(reg,reg)    "# div\n"  35
reg: DIVU4(reg,reg)    "# div\n"  35

reg: MODI1(reg,reg)    "# mod\n"  35
reg: MODU1(reg,reg)    "# mod\n"  35
reg: MODI4(reg,reg)    "# mod\n"  35
reg: MODU4(reg,reg)    "# mod\n"  35

reg: NEGI1(reg)        "# neg\n"  2
reg: NEGI4(reg)        "# neg\n"  2

reg: BANDI1(reg,reg)   "# and\n"  2
reg: BANDU1(reg,reg)   "# and\n"  2
reg: BORI1(reg,reg)    "# or\n"   2
reg: BORU1(reg,reg)    "# or\n"   2
reg: BXORI1(reg,reg)   "# xor\n"  2
reg: BXORU1(reg,reg)   "# xor\n"  2
reg: BCOMI1(reg)       "# notr\n" 1
reg: BCOMU1(reg)       "# notr\n" 1

reg: BANDI4(reg,reg)   "# and\n"  2
reg: BANDU4(reg,reg)   "# and\n"  2
reg: BORI4(reg,reg)    "# or\n"   2
reg: BORU4(reg,reg)    "# or\n"   2
reg: BXORI4(reg,reg)   "# xor\n"  2
reg: BXORU4(reg,reg)   "# xor\n"  2
reg: BCOMI4(reg)       "# notr\n" 1
reg: BCOMU4(reg)       "# notr\n" 1

rc6: CNSTI1  "%a"  range(a, 0, 63)
rc6: CNSTU1  "%a"  range(a, 0, 63)
rc6: CNSTI4  "%a"  range(a, 0, 63)
rc6: CNSTU4  "%a"  range(a, 0, 63)

reg: LSHI1(reg,rc6)    "COPY %c %0\nSHLV %c %1\n"  2
reg: LSHU1(reg,rc6)    "COPY %c %0\nSHLV %c %1\n"  2
reg: LSHI1(reg,reg)    "# lsh_var\n"  2
reg: LSHU1(reg,reg)    "# lsh_var\n"  2
reg: RSHI1(reg,rc6)    "COPY %c %0\nSHRAV %c %1\n"  2
reg: RSHU1(reg,rc6)    "COPY %c %0\nSHRV %c %1\n"   2
reg: RSHI1(reg,reg)    "# rsha_var\n" 2
reg: RSHU1(reg,reg)    "# rshl_var\n" 2

reg: LSHI4(reg,rc6)    "COPY %c %0\nSHLV %c %1\n"  2
reg: LSHU4(reg,rc6)    "COPY %c %0\nSHLV %c %1\n"  2
reg: LSHI4(reg,reg)    "# lsh_var\n"  2
reg: LSHU4(reg,reg)    "# lsh_var\n"  2
reg: RSHI4(reg,rc6)    "COPY %c %0\nSHRAV %c %1\n"  2
reg: RSHU4(reg,rc6)    "COPY %c %0\nSHRV %c %1\n"   2
reg: RSHI4(reg,reg)    "# rsha_var\n" 2
reg: RSHU4(reg,reg)    "# rshl_var\n" 2

reg: LOADI1(reg)  "COPY %c %0\n"  move(a)
reg: LOADU1(reg)  "COPY %c %0\n"  move(a)
reg: LOADI2(reg)  "COPY %c %0\n"  move(a)
reg: LOADU2(reg)  "COPY %c %0\n"  move(a)
reg: LOADP1(reg)  "COPY %c %0\n"  move(a)
reg: LOADI4(reg)  "COPY %c %0\n"  move(a)
reg: LOADU4(reg)  "COPY %c %0\n"  move(a)
reg: LOADP4(reg)  "COPY %c %0\n"  move(a)

reg: CVII1(reg)  "COPY %c %0\n"  move(a)
reg: CVUI1(reg)  "COPY %c %0\n"  move(a)
reg: CVIU1(reg)  "COPY %c %0\n"  move(a)
reg: CVUU1(reg)  "COPY %c %0\n"  move(a)
reg: CVPU1(reg)  "COPY %c %0\n"  move(a)
reg: CVUP1(reg)  "COPY %c %0\n"  move(a)
reg: CVPP1(reg)  "COPY %c %0\n"  move(a)

reg: CVII4(reg)  "COPY %c %0\n"  move(a)
reg: CVUI4(reg)  "COPY %c %0\n"  move(a)
reg: CVIU4(reg)  "COPY %c %0\n"  move(a)
reg: CVUU4(reg)  "COPY %c %0\n"  move(a)
reg: CVPU4(reg)  "COPY %c %0\n"  move(a)
reg: CVUP4(reg)  "COPY %c %0\n"  move(a)
reg: CVPP4(reg)  "COPY %c %0\n"  move(a)

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

stmt: EQI4(reg,reg)   "CMPRR %0 %1\nJMPE %a:\n"    2
stmt: EQU4(reg,reg)   "CMPRR %0 %1\nJMPE %a:\n"    2
stmt: NEI4(reg,reg)   "CMPRR %0 %1\nJMPNE %a:\n"   2
stmt: NEU4(reg,reg)   "CMPRR %0 %1\nJMPNE %a:\n"   2

stmt: LTI4(reg,reg)   "CMPRR %0 %1\nJMPLT %a:\n"   2
stmt: LEI4(reg,reg)   "CMPRR %0 %1\nJMPLE %a:\n"   2
stmt: GTI4(reg,reg)   "CMPRR %0 %1\nJMPGT %a:\n"   2
stmt: GEI4(reg,reg)   "CMPRR %0 %1\nJMPGE %a:\n"   2

stmt: LTU4(reg,reg)   "CMPRR %0 %1\nJMPULT %a:\n"   2
stmt: LEU4(reg,reg)   "CMPRR %0 %1\nJMPULE %a:\n"    2
stmt: GTU4(reg,reg)   "CMPRR %0 %1\nJMPUGT %a:\n"    2
stmt: GEU4(reg,reg)   "CMPRR %0 %1\nJMPUGE %a:\n"    2

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

reg:  CALLI4(acon)      "CALL %0:\n"    1
reg:  CALLU4(acon)      "CALL %0:\n"    1
reg:  CALLP4(acon)      "CALL %0:\n"    1

reg:  CALLI4(reg)       "CALLR %0\n"    2
reg:  CALLU4(reg)       "CALLR %0\n"    2
reg:  CALLP4(reg)       "CALLR %0\n"    2

stmt: RETI1(reg)       "# ret\n"      1
stmt: RETU1(reg)       "# ret\n"      1
stmt: RETP1(reg)       "# ret\n"      1
stmt: RETV(reg)        "# ret\n"      1
stmt: RETI4(reg)       "# ret\n"      1
stmt: RETU4(reg)       "# ret\n"      1
stmt: RETP4(reg)       "# ret\n"      1

stmt: ARGI1(reg)       "# arg\n"      1
stmt: ARGU1(reg)       "# arg\n"      1
stmt: ARGP1(reg)       "# arg\n"      1
stmt: ARGI4(reg)       "# arg\n"      1
stmt: ARGU4(reg)       "# arg\n"      1
stmt: ARGP4(reg)       "# arg\n"      1

stmt: ARGB(INDIRB(reg))       "# argb %0\n"      1
stmt: ASGNB(reg,INDIRB(reg))  "# asgnb %0 %1\n"  1

/* === Size-8 grammar rules (sizeof(int)=sizeof(void*)=8) === */

reg: INDIRI8(VREGP)   "# read register\n"
reg: INDIRU8(VREGP)   "# read register\n"
reg: INDIRP8(VREGP)   "# read register\n"

stmt: ASGNI8(VREGP,reg)  "# write register\n"
stmt: ASGNU8(VREGP,reg)  "# write register\n"
stmt: ASGNP8(VREGP,reg)  "# write register\n"

reg: CNSTI8  "SETR %c %a\n"  1
reg: CNSTU8  "SETR %c %a\n"  1
reg: CNSTP8  "SETR %c %a\n"  1

acon: CNSTI8   "%a"
acon: CNSTU8   "%a"
acon: CNSTP8   "%a"
acon: ADDRGP8  "%a"

reg: ADDRFP8  "# addr FP\n"  2
reg: ADDRLP8  "# addr LP\n"  2

reg: INDIRI8(reg)   "MEMGET64 %c %0\n"  1
reg: INDIRU8(reg)   "MEMGET64 %c %0\n"  1
reg: INDIRP8(reg)   "MEMGET64 %c %0\n"  1

stmt: ASGNI8(reg,reg)  "MEMSET64 %1 %0\n"  1
stmt: ASGNU8(reg,reg)  "MEMSET64 %1 %0\n"  1
stmt: ASGNP8(reg,reg)  "MEMSET64 %1 %0\n"  1

reg: ADDI8(reg,reg)   "# add\n"  2
reg: ADDU8(reg,reg)   "# add\n"  2
reg: ADDP8(reg,reg)   "# add\n"  2

reg: SUBI8(reg,reg)   "# sub\n"  2
reg: SUBU8(reg,reg)   "# sub\n"  2
reg: SUBP8(reg,reg)   "# sub\n"  2

reg: MULI8(reg,reg)   "# mul\n"  3
reg: MULU8(reg,reg)   "# mul\n"  3

reg: DIVI8(reg,reg)   "# div\n"  35
reg: DIVU8(reg,reg)   "# div\n"  35

reg: MODI8(reg,reg)   "# mod\n"  35
reg: MODU8(reg,reg)   "# mod\n"  35

reg: NEGI8(reg)       "# neg\n"  2

reg: BANDI8(reg,reg)  "# and\n"  2
reg: BANDU8(reg,reg)  "# and\n"  2
reg: BORI8(reg,reg)   "# or\n"   2
reg: BORU8(reg,reg)   "# or\n"   2
reg: BXORI8(reg,reg)  "# xor\n"  2
reg: BXORU8(reg,reg)  "# xor\n"  2
reg: BCOMI8(reg)      "# notr\n" 1
reg: BCOMU8(reg)      "# notr\n" 1

rc6: CNSTI8  "%a"  range(a, 0, 63)
rc6: CNSTU8  "%a"  range(a, 0, 63)

reg: LSHI8(reg,rc6)   "COPY %c %0\nSHLV %c %1\n"   2
reg: LSHU8(reg,rc6)   "COPY %c %0\nSHLV %c %1\n"   2
reg: LSHI8(reg,reg)   "# lsh_var\n"  2
reg: LSHU8(reg,reg)   "# lsh_var\n"  2
reg: RSHI8(reg,rc6)   "COPY %c %0\nSHRAV %c %1\n"  2
reg: RSHU8(reg,rc6)   "COPY %c %0\nSHRV %c %1\n"   2
reg: RSHI8(reg,reg)   "# rsha_var\n" 2
reg: RSHU8(reg,reg)   "# rshl_var\n" 2

reg: LOADI8(reg)  "COPY %c %0\n"  move(a)
reg: LOADU8(reg)  "COPY %c %0\n"  move(a)
reg: LOADP8(reg)  "COPY %c %0\n"  move(a)

reg: CVII8(reg)  "COPY %c %0\n"  move(a)
reg: CVUI8(reg)  "COPY %c %0\n"  move(a)
reg: CVIU8(reg)  "COPY %c %0\n"  move(a)
reg: CVUU8(reg)  "COPY %c %0\n"  move(a)
reg: CVPU8(reg)  "COPY %c %0\n"  move(a)
reg: CVUP8(reg)  "COPY %c %0\n"  move(a)
reg: CVPP8(reg)  "COPY %c %0\n"  move(a)

stmt: EQI8(reg,reg)   "CMPRR %0 %1\nJMPE %a:\n"     2
stmt: EQU8(reg,reg)   "CMPRR %0 %1\nJMPE %a:\n"     2
stmt: NEI8(reg,reg)   "CMPRR %0 %1\nJMPNE %a:\n"    2
stmt: NEU8(reg,reg)   "CMPRR %0 %1\nJMPNE %a:\n"    2
stmt: LTI8(reg,reg)   "CMPRR %0 %1\nJMPLT %a:\n"    2
stmt: LEI8(reg,reg)   "CMPRR %0 %1\nJMPLE %a:\n"    2
stmt: GTI8(reg,reg)   "CMPRR %0 %1\nJMPGT %a:\n"    2
stmt: GEI8(reg,reg)   "CMPRR %0 %1\nJMPGE %a:\n"    2
stmt: LTU8(reg,reg)   "CMPRR %0 %1\nJMPULT %a:\n"   2
stmt: LEU8(reg,reg)   "CMPRR %0 %1\nJMPULE %a:\n"   2
stmt: GTU8(reg,reg)   "CMPRR %0 %1\nJMPUGT %a:\n"   2
stmt: GEU8(reg,reg)   "CMPRR %0 %1\nJMPUGE %a:\n"   2

reg:  CALLI8(acon)   "CALL %0:\n"   1
reg:  CALLU8(acon)   "CALL %0:\n"   1
reg:  CALLP8(acon)   "CALL %0:\n"   1
reg:  CALLI8(reg)    "CALLR %0\n"   2
reg:  CALLU8(reg)    "CALLR %0\n"   2
reg:  CALLP8(reg)    "CALLR %0\n"   2

stmt: RETI8(reg)   "# ret\n"  1
stmt: RETU8(reg)   "# ret\n"  1
stmt: RETP8(reg)   "# ret\n"  1

stmt: ARGI8(reg)   "# arg\n"  1
stmt: ARGU8(reg)   "# arg\n"  1
stmt: ARGP8(reg)   "# arg\n"  1

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
 */
static void emit2(Node p) {
    int dst = -1;
    int op = specific(p->op);

    /* Only get dest register for nodes that produce a value (have a result) */
    if (p->syms[RX] && p->syms[RX]->x.regnode)
        dst = getregnum(p);

    switch (op) {

    /* --- Address computation for frame-relative access (byte offsets) --- */
    case ADDRF+P: {
        /* Parameter address: FP + off (off is negative bytes from FP) */
        int off = p->syms[0]->x.offset;
        print("COPY %s P\nMINUSV %s %s\n",
            ireg[dst]->x.name, ireg[dst]->x.name,
            imm(-off));
        break;
    }
    case ADDRL+P: {
        /* Local variable address: FP + off (off is negative bytes from FP) */
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
        print("NOTR %s\n", ireg[dst]->x.name);
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

    /* --- Function arguments ---
     * Overflow args (index >= 4) are stored at byte offsets from SP.
     * stkoff is the byte offset returned by mkactual(8,8): 0, 8, 16, ...
     */
    case ARG+I: case ARG+U: case ARG+P: {
        int argno = p->x.argno;
        if (argno >= 4) {
            int src    = getregnum(p->x.kids[0]);
            int stkoff = p->syms[2]->u.c.v.i; /* byte offset */
            print("GETSP N\n");
            print("STIDX64 %s N %d\n", ireg[src]->x.name, stkoff);
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
    /* Each arg is 4 bytes, aligned to 4 bytes.
     * mkactual returns the byte offset for this arg: 0, 4, 8, ... */
    p->syms[2] = intconst(mkactual(8, 8));
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

    /* Process parameters — each is 4 bytes (sizeof(int)=4).
     * lcc computes byte offsets using roundup(offset + type->size, type->align).
     * Offsets are negated so frame access is always FP + negative_offset:
     *   param 0: FP-4, param 1: FP-8, etc.
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

    /* Generate code — offset not reset so locals follow param slots */
    gencode(caller, callee);

    /* Ensure maxoffset covers all param frame slots (8 bytes each) */
    if (maxoffset < numparams * 8)
        maxoffset = numparams * 8;

    /* Count callee-saved registers used */
    usedmask[IREG] &= INTVAR;
    saved = 0;
    for (i = REG_E; i <= REG_H; i++)
        if (usedmask[IREG] & (1 << i))
            saved++;

    /* Frame size in bytes: locals/spills + saved regs + outgoing args.
     * ADDSP takes a byte count (hardware is byte-addressed).
     */
    framesize_actual = maxargoffset + maxoffset + saved * 8;

    /* === Emit prologue === */
    curfunc = f->x.name;
    segment(CODE);
    print("%s:\n", f->x.name);

    /* Save old FP and set up new frame using hardware stack */
    print("PUSH P\n");              /* save old FP on DDR2 stack */
    print("GETSP P\n");             /* FP = SP (points to saved old FP) */

    /* Allocate frame: ADDSP takes byte count (negative = grow stack) */
    if (framesize_actual > 0)
        print("ADDSP %s\n", imm(-framesize_actual));

    /* Save callee-saved registers at byte offsets below locals.
     * Slot 0: FP-(maxoffset+4), slot 1: FP-(maxoffset+8), etc.
     */
    {
        int slot = 0;
        for (i = REG_E; i <= REG_H; i++) {
            if (usedmask[IREG] & (1 << i)) {
                print("STIDX64 %s P %s\n", ireg[i]->x.name,
                    imm(-(maxoffset + (slot + 1) * 8)));
                slot++;
            }
        }
    }

    /* Save register arguments (0-3) to frame if needed.
     * p->x.offset is a negative byte offset from FP.
     */
    for (i = 0; i < 4 && callee[i]; i++) {
        Symbol p = callee[i];
        Symbol q = caller[i];
        if (p->sclass != REGISTER
            || p->sclass != q->sclass
            || p->type != q->type) {
            print("// save arg %d\n", i);
            print("STIDX64 %s P %s\n", ireg[i]->x.name,
                imm(p->x.offset));
            if (p->sclass != REGISTER)
                p->sclass = q->sclass = AUTO;
        }
    }

    /* Move register args to their callee-saved register variables. */
    for (i = 0; i < 4 && callee[i]; i++) {
        Symbol p = callee[i];
        if (p->sclass == REGISTER && p->x.regnode
            && p->x.regnode->number != i) {
            print("COPY %s %s\n", ireg[p->x.regnode->number]->x.name,
                ireg[i]->x.name);
        }
    }

    /* Copy overflow arguments (>=4) from caller's stack to local frame.
     * After CALL (pushes ret addr) + PUSH P (saves old FP):
     *   FP+0  = saved old FP  (PUSH P)
     *   FP+4  = return addr   (CALL pushed it)
     *   FP+8  = overflow arg 0 (first stack arg)
     *   FP+12 = overflow arg 1
     * LDIDX uses byte offsets; each entry is 4 bytes (1 word).
     */
    for (i = 4; i < numparams; i++) {
        Symbol p = callee[i];
        if (p->sclass != REGISTER) {
            print("// copy overflow arg %d\n", i);
            print("LDIDX64 N P %d\n", (i - 4) * 8 + 16);
            print("STIDX64 N P %s\n", imm(p->x.offset));
        }
    }

    /* === Emit body === */
    emitcode();

    /* === Emit epilogue === */
    {
        int slot = 0;
        for (i = REG_E; i <= REG_H; i++) {
            if (usedmask[IREG] & (1 << i)) {
                print("LDIDX64 %s P %s\n", ireg[i]->x.name,
                    imm(-(maxoffset + (slot + 1) * 8)));
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
    int i;
    unsigned int word;
    /* Pack 4 bytes big-endian into each 32-bit word.
     * "ABCD" -> 0x41424344.  Final word is zero-padded.
     * Null terminator is included in n by lcc. */
    for (i = 0; i < n; i += 4) {
        word  = ((unsigned char)str[i+0])                           << 24;
        word |= (i+1 < n ? (unsigned char)str[i+1] : 0u)           << 16;
        word |= (i+2 < n ? (unsigned char)str[i+2] : 0u)           <<  8;
        word |= (i+3 < n ? (unsigned char)str[i+3] : 0u);
        print(".word 0x%x\n", word);
    }
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
        /* p->type->size is in bytes; assembler .space takes words */
        print("%s:\n", p->x.name);
        print("#%s %d\n", p->x.name, (p->type->size + 7) / 8);
    } else {
        print("%s:\n", p->x.name);
    }
}

static void segment(int n) {
    cseg = n;
    /* .kla format: no segment directives needed */
}

static void space(int n) {
    /* n is in bytes; assembler .space takes words */
    if (cseg != BSS)
        print(".space %d\n", (n + 7) / 8);
}

/* === Block copy support ===
 * blkfetch/blkstore use byte offsets (STIDX/LDIDX take byte offsets).
 * size is the alignment unit in bytes.
 */

static void blkfetch(int size, int off, int reg, int tmp) {
    /* off is a byte offset; use word load for 4-byte aligned blocks */
    if (size == 4)
        print("LDIDX64 %s %s %d\n", ireg[tmp]->x.name, ireg[reg]->x.name, off * 8);
    else
        print("MEMGET8 %s %s\n", ireg[tmp]->x.name, ireg[reg]->x.name);
}

static void blkstore(int size, int off, int reg, int tmp) {
    if (size == 4)
        print("STIDX64 %s %s %d\n", ireg[tmp]->x.name, ireg[reg]->x.name, off * 8);
    else
        print("MEMSET8 %s %s\n", ireg[tmp]->x.name, ireg[reg]->x.name);
}

static void blkloop(int dreg, int doff, int sreg, int soff, int size, int tmps[]) {
    int lab = genlabel(1);
    print("SETR %s %d\n", ireg[tmps[2]]->x.name, size);
    print("%s_SH_%d:\n", curfunc, lab);
    blkcopy(dreg, doff, sreg, soff, 1, tmps);
    /* Advance byte addresses by 4 (one word per iteration) */
    print("ADDV %s 8\n", ireg[sreg]->x.name);
    print("ADDV %s 8\n", ireg[dreg]->x.name);
    print("DECR %s\n", ireg[tmps[2]]->x.name);
    print("CMPRV %s 0\n", ireg[tmps[2]]->x.name);
    print("JMPNZ %s_SH_%d:\n", curfunc, lab);
}

/* === Stab stubs (no debug info for now) === */

static char rcsid[] = "$Id: klacpu.md$";

/* === THE INTERFACE RECORD === */

Interface klacpuIR = {
    1, 1, 0,  /* char:       size=1 byte,  align=1 byte  */
    4, 4, 0,  /* short:      size=4 bytes, align=4 bytes (no 16-bit hw) */
    8, 8, 0,  /* int:        size=8 bytes, align=8 bytes (native 64-bit word) */
    8, 8, 0,  /* long:       size=8 bytes, align=8 bytes */
    8, 8, 0,  /* longlong:   size=8 bytes, align=8 bytes */
    8, 8, 1,  /* float:      size=8 bytes, align=8, outofline=1 (no FP hw) */
    8, 8, 1,  /* double:     size=8 bytes, align=8, outofline=1 */
    8, 8, 1,  /* longdouble: same */
    8, 8, 0,  /* T*:         size=8 bytes, align=8 bytes */
    0, 8, 0,  /* struct:     size=0, align=8 bytes */
    0,        /* little_endian = 0 (big-endian byte packing in strings) */
    0,        /* mulops_calls = 0 (hardware mul/div) */
    0,        /* wants_callb = 0 */
    1,        /* wants_argb = 1 */
    1,        /* left_to_right = 1 */
    0,        /* wants_dag = 0 */
    1,        /* unsigned_char = 1 (MEMGET8 zero-extends; no sign-ext needed) */
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
        4,      /* max_unaligned_load = 4 bytes (word) */
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
