#!/bin/sh
# The flat build's modules: syms.inc must be what tools/x86syms.py writes, and a constant or macro
# two modules share must be in a header.
cd "$(dirname "$0")/../.." || exit 1
if python3 tools/x86syms.py check; then
    echo "ok   test/tools/x86syms.sh"
else
    echo "FAIL test/tools/x86syms.sh"
    exit 1
fi
