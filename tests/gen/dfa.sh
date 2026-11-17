#!/bin/sh
# The flat build's lexer and parser tables against their models: the old hand lexer and the old
# statement scripts. dfa.inc must be what the generator writes.
cd "$(dirname "$0")/../.." || exit 1
if python3 tools/gen_dfa.py check >/dev/null; then
    echo "ok   tests/gen/dfa.sh"
else
    echo "FAIL tests/gen/dfa.sh"
    exit 1
fi
