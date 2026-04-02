#!/bin/bash
# Run all test cases and report results
# Usage: ./run_tests.sh

RCC=build/rcc
CPP=build/cpp
TARGET=klacpu
PASS=0
FAIL=0

for f in tests/test_*.c; do
    name=$(basename "$f" .c)
    if $CPP "$f" | $RCC -target=$TARGET > /dev/null 2>&1; then
        echo "PASS: $name"
        PASS=$((PASS + 1))
    else
        echo "FAIL: $name"
        FAIL=$((FAIL + 1))
    fi
done

echo ""
echo "Results: $PASS passed, $FAIL failed"
