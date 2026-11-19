#!/bin/sh
# --check FILE reports every compile error and runs nothing: a syntax check once meant running the
# program under timeout, which stopped at its first error and ran the rest.
set -e
B=$(cd "$(dirname "${1:-build/host/basic}")" && pwd)/$(basename "${1:-build/host/basic}")
D=$(mktemp -d "${TMPDIR:-/tmp}/check.XXXXXX")
trap 'rm -rf "${D:?}"' EXIT
printf 'PRINT "RAN"\nX = (\nGOTO 999\nY = 1 +\n' > "$D/BAD.BAS"
printf 'PRINT "RAN"\n' > "$D/GOOD.BAS"
printf 'FOR I = 1 TO 2\n' > "$D/BLOCK.BAS"
(cd "$D" && "$B" --check BAD.BAS < /dev/null > out) && { echo "check: BAD.BAS exit 0"; exit 1; }
out=$(tr -d '\r' < "$D/out")
[ "$(echo "$out" | grep -c '^BAD.BAS:[0-9]*: Syntax error')" = 2 ] || { echo "check: got '$out'"; exit 1; }
echo "$out" | grep -q '^BAD.BAS:3: Label not defined' || { echo "check: no Label not defined in '$out'"; exit 1; }
! echo "$out" | grep -q RAN || { echo "check: BAD.BAS ran"; exit 1; }
out=$(cd "$D" && "$B" --check GOOD.BAS < /dev/null | tr -d '\r')
[ -z "$out" ] || { echo "check: GOOD.BAS got '$out'"; exit 1; }
(cd "$D" && "$B" --check BLOCK.BAS < /dev/null > /dev/null) && { echo "check: BLOCK.BAS exit 0"; exit 1; }
echo ok
