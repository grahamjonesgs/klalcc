# lcc with KLACPU Backend

C compiler for the FPGA_CPU_32_DDR_cache custom CPU.

## Build the Compiler

```bash
mkdir lcc-klacpu && cd lcc-klacpu
tar xzf lcc-klacpu.tar.gz

cat > custom.mk << 'EOF'
BUILDDIR=build
TARGET=klacpu
CFLAGS=-g -std=gnu89 -Wno-implicit-function-declaration -Wno-implicit-int
EOF

mkdir -p build/klacpu/tst
make lburg
make rcc
bash run_tests.sh    # 10/10 should pass
```

## Usage

The compiler outputs .kla assembly directly.

```bash
# Compile C to .kla
build/rcc -target=klacpu myprogram.c > myprogram.kla

# Assemble via crt0.kla (edit INCLUDE path first)
your_assembler crt0.kla
```

### Workflow

```
myprogram.c
    |  build/rcc -target=klacpu
    v
myprogram.kla
    |
    v
crt0.kla  -->  assembler  -->  hex  -->  UART upload  -->  FPGA
  INCLUDE myprogram.kla
  INCLUDE lib/libc.kla
  INCLUDE lib/uart_stubs.kla
```

### crt0.kla

Edit the INCLUDE line to point to your compiled program:

```
_start
SETR P 0xFFFFF
COPY O P
DELAYV 0x0500
CALL main:
TXR M
NEWLINE
HALT

INCLUDE myprogram.kla
INCLUDE lib/libc.kla
INCLUDE lib/uart_stubs.kla
```

## C Library (lib/libc.h)

Available when you include lib/libc.kla:

| Category | Functions |
|----------|-----------|
| Output | putchar, puts, print_str, print_int, print_unsigned, print_hex, newline |
| String | strlen, strcmp, strcpy |
| Memory | memset, memcpy |
| Utility | abs, min, max, swap |
| Low-level | _uart_tx_hex, _uart_newline, _uart_tx_char |

### Recompiling libc

If you modify lib/libc.c:
```bash
build/rcc -target=klacpu lib/libc.c > lib/libc.kla
```

## Memory Model

- Word-addressed: each address = one 32-bit word
- sizeof(char) = sizeof(int) = sizeof(void*) = 1
- CHAR_BIT = 32, pointer arithmetic: p+1 = next word
- Strings: one character per word (ASCII in low bits)

## Register Convention

| Registers | Role |
|-----------|------|
| A-D (0-3) | Arguments, caller-saved |
| E-H (4-7) | Callee-saved (register variables) |
| I-L (8-11) | Caller-saved temporaries |
| M (12) | Return value |
| N (13) | Scratch (not allocatable) |
| O (14) | Frame pointer |
| P (15) | Stack pointer |

## Assembler Requirements

| Feature | Example | Purpose |
|---------|---------|---------|
| Labels | main: | Function and branch targets |
| Label refs in SETR | SETR A mydata | Pointers to global data |
| Negative immediates | STIDX G O -1 | Frame-relative access |
| .word VALUE | .word 0x48 | Initialized data |
| .space N | .space 4 | Zero-filled data |
| #NAME SIZE | #counter 1 | BSS allocation |
| INCLUDE file | INCLUDE lib/libc.kla | Include library |

Programs without initialized globals work without .word support.

## Demo Programs

| File | Description | Needs .word? |
|------|-------------|-------------|
| tests/demo_hex.c | Factorial, GCD, fibonacci, primes (hex output) | No |
| tests/demo.c | Full text output with string constants | Yes |

## Files

```
src/klacpu.md        Backend machine description
src/bind.c           Target binding (modified)
makefile             Build rules (modified)
lib/libc.c           C library source
lib/libc.h           C library header
lib/libc.kla         Precompiled C library
lib/uart_stubs.kla   UART assembly stubs
crt0.kla             Runtime startup template
run_tests.sh         Compiler test suite
tests/test_*.c       10 test programs
tests/demo_hex.c     Hex output demo
tests/demo.c         Text output demo
```
