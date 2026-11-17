#!/bin/sh
# io/msg.inc must be what tools/gen_messages.py writes from llrm's messages.
cd "$(dirname "$0")/../.." || exit 1
if python3 tools/gen_messages.py check >/dev/null; then
    echo "ok   tests/gen/messages.sh"
else
    echo "FAIL tests/gen/messages.sh"
    exit 1
fi
