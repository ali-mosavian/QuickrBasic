#!/bin/sh
# The flat build's modules: syms.inc must be what tools/asm_syms.py writes, and a constant or macro
# two modules share must be in a header.
cd "$(dirname "$0")/../.." || exit 1
if python3 tools/asm_syms.py check; then
    echo "ok   tests/gen/asm_syms.sh"
else
    echo "FAIL tests/gen/asm_syms.sh"
    exit 1
fi
