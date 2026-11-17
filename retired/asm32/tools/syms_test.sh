#!/bin/sh
# syms.inc must be what tools/syms.py writes, and a constant or macro two modules share must be in a header.
cd "$(dirname "$0")/.." || exit 1
if python3 tools/syms.py check; then
    echo "ok   tools/syms_test.sh"
else
    echo "FAIL tools/syms_test.sh"
    exit 1
fi
