#!/bin/sh
# The flat build's lexer and parser tables against their models: the old hand lexer and the old
# statement scripts. dfa.inc must be what the generator writes.
cd "$(dirname "$0")/../.." || exit 1
if python3 tools/x86dfa.py check >/dev/null; then
    echo "ok   test/tools/x86dfa.sh"
else
    echo "FAIL test/tools/x86dfa.sh"
    exit 1
fi
