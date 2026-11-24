#!/bin/sh
# gfx.qbx, a package of parts, from lib/ beside the interpreter: each IMPORT form binds its parts'
# entries, CONSTs and records; in QuickrBasic's dialect DRAW and OPEN are names; a part it lacks
# is File not found.
# Usage: gfx.sh BASIC, the build's lib/gfx.qbx beside it.
set -e
B=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
D=$(mktemp -d "${TMPDIR:-/tmp}/gfx.XXXXXX")
trap 'rm -rf "${D:?}"' EXIT

# check NAME WANT LINE...: the program of LINEs prints WANT
check() {
    name=$1 want=$2
    shift 2
    printf '%s\n' "$@" > "$D/t.bas"
    out=$(cd "$D" && "$B" t.bas < /dev/null 2>&1 | tr -d '\r') || true
    [ "$out" = "$want" ] || { echo "gfx: $name: got '$out'"; exit 1; }
}

check 'dotted' ' 5  1 ' \
    'import gfx.display, gfx.surface, gfx.draw' \
    'canvas& = gfx.display.open(8, 4, 8, 0)' \
    'gfx.draw.rect canvas&, 1, 1, 3, 2, 5, gfx.draw.FILL' \
    'print gfx.surface.point(canvas&, 2, 2); gfx.display.draws'

check 'from' ' 7  1  4  1  2 ' \
    'from gfx import surface' \
    'from gfx.display import draws, get_drawn, Command' \
    'import gfx.display as d' \
    'from gfx.draw import rect' \
    'canvas& = d.open(4, 4, 8, 0)' \
    'rect canvas&, 1, 2, 2, 3, 7, 4' \
    'dim c as Command' \
    'get_drawn 1, c' \
    'print surface.point(canvas&, 1, 2); draws; c.kind; c.x; c.y'

check 'quickrbasic' ' 6  1 ' \
    'option dialect quickrbasic' \
    'from gfx import display, surface, draw' \
    'canvas& = display.open(4, 4, 8, 0)' \
    'draw.rect canvas&, 0, 0, 2, 2, 6, draw.FILL' \
    'n = surface.point(canvas&, 1, 1)' \
    'option dialect qb' \
    'print n; display.draws'

check 'no such part' "$(printf 't.bas:1: File not found\n  import gfx.nosuch\n  ^')" \
    'import gfx.nosuch'
echo ok
