#!/bin/sh
# io/msg.inc must be what tools/x86msg.py writes from llrm's messages.
cd "$(dirname "$0")/../.." || exit 1
if python3 tools/x86msg.py check >/dev/null; then
    echo "ok   test/tools/x86msg.sh"
else
    echo "FAIL test/tools/x86msg.sh"
    exit 1
fi
