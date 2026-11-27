#!/bin/sh
# A native module (tests/ext/qxtest): each kind of argument and result through its code, its error
# at the calling line, its start once a run, its clock finer than a millisecond, its audio stream
# refused with no device; the same from Quickr, its error's reason said, a list passed where it takes an array. Then the
# .qbx files that cannot bind: no slice for this machine, not a .qbx, an entry its code lacks, a
# DECLARE its code's kinds disagree with. With audio, a device (QB_AUDIO) plays its stream.
# Usage: native.sh BASIC SLICE TAG [audio], the slice qxtest's for this machine's tag.
set -e
B=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
S=$(cd "$(dirname "$2")" && pwd)/$(basename "$2")
TAG=$3
I=$(cd "$(dirname "$0")/../ext" && pwd)/qxtest.bas
Q=$(cd "$(dirname "$0")/../.." && pwd)/tools/qbx.py
D=$(mktemp -d "${TMPDIR:-/tmp}/native.XXXXXX")
trap 'rm -rf "${D:?}"' EXIT
pack() { ${PY:-python3} "$Q" pack "$@"; }
run() { (cd "$D" && "$B" t.bas < /dev/null 2>&1 | tr -d '\r'); }

pack "$D/qxtest.qbx" "$I" "$TAG=$S"
cat > "$D/t.bas" << 'EOF'
from qxtest import *
print add%(2, 3); half#(5)
print shout$("hi there")
n% = 41
bump n%
print n%
dim p as pt
p.x = 7
move p, 3
print p.x
dim a(1 to 3) as single
a(1) = 1.5
a(2) = 2
a(3) = 3
print total!(a())
print twice%(21); runs%
print audio%; played& \ 1000000
on error goto caught
fail 5
print "after"
end
caught:
print "err"; err
resume next
EOF
want=$(printf ' 5  2.5 \nHI THERE\n 42 \n 10 \n 6.5 \n 42  1 \n 0  0 \nerr 5 \nafter')
out=$(run)
[ "$out" = "$want" ] || { echo "native: got '$out'"; exit 1; }

# the clock resolves below a millisecond: the web's stepped by whole ones
printf 'from qxtest import *\nfor i%% = 1 to 100\nif fraction%%() then f%% = -1\nnext\nprint f%%\n' > "$D/c.bas"
out=$(cd "$D" && "$B" c.bas < /dev/null 2>&1 | tr -d '\r')
[ "$out" = "-1 " ] || { echo "native: clock: got '$out'"; exit 1; }

cat > "$D/q.bas" << 'EOF'
option dialect quickr
from qxtest import add, half, shout, bump, move, pt, total, items, twice, runs, fail
print add(2, 3); " "; half(5)
print shout("hi there")
var n as int = 41
bump(n)
var p = pt(7, 0)
move(p, 3)
var a as single[]
a.push(1.5)
a.push(2)
a.push(3)
print n; " "; p.x; " "; total(a); " "; items(a); " "; twice(21); " "; runs()
fail(5)
EOF
want=$(printf '5 2.5\nHI THERE\n42 10 6.5 3 42 1\nq.bas:14: Illegal function call\n  fail(5)\nqxtest was asked to fail')
out=$(cd "$D" && "$B" q.bas < /dev/null 2>&1 | tr -d '\r') || true
[ "$out" = "$want" ] || { echo "native: from Quickr: got '$out'"; exit 1; }

if [ "$4" = audio ]; then
    printf 'from qxtest import *\nprint audio%%\nt! = timer\ndo\nloop until timer - t! > .5\nprint played& > 0\n' \
        > "$D/a.bas"
    out=$(cd "$D" && QB_AUDIO=1 "$B" a.bas < /dev/null 2>&1 | tr -d '\r')
    [ "$out" = "$(printf -- '-1 \n-1 ')" ] || { echo "native: audio: got '$out'"; exit 1; }
fi

expect() { # the program's output has $1, $2 says which .qbx
    out=$(run) || true
    echo "$out" | grep -q "$1" || { echo "native: $2: got '$out'"; exit 1; }
}

pack "$D/qxtest.qbx" "$I" "dos-i386=$S"
expect 'Advanced feature unavailable' 'no slice for this machine'
printf 'not a qbx\n' > "$D/qxtest.qbx"
expect '^t.bas:1: Advanced feature unavailable' 'not a .qbx'
{ cat "$I"; echo 'declare sub nosuch cdecl ()'; } > "$D/more.bas"
pack "$D/qxtest.qbx" "$D/more.bas" "$TAG=$S"
expect 'Subprogram not defined' 'an entry its code lacks'
sed 's/(byval a%, byval b%)/(byval a%, byval b#)/' "$I" > "$D/other.bas"
pack "$D/qxtest.qbx" "$D/other.bas" "$TAG=$S"
expect 'Parameter type mismatch' 'kinds its code disagrees with'
echo ok
