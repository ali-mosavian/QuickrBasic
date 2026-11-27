#!/bin/sh
# gfx.qbx, a package of parts, from lib/ beside the interpreter: each IMPORT form binds its parts'
# entries, CONSTs and records; DRAW and OPEN are names after OPTION DISABLE, and an interface's
# disabled words stay its own; Quickr's import and from bind them too; a part it lacks is File not
# found.
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

check 'disabled' ' 6  1 ' \
    'option disable draw, point' \
    'from gfx import display, surface, draw' \
    'from gfx.surface import point' \
    'canvas& = display.open(4, 4, 8, 0)' \
    'draw.rect canvas&, 0, 0, 2, 2, 6, draw.FILL' \
    'print point(canvas&, 1, 1); display.draws'

check 'interface words its own' ' 3 ' \
    'import gfx.draw' \
    'clear' \
    'x = 3' \
    'print x'

check 'quickr' "$(printf '5 1\n5')" \
    "option dialect quickr" \
    'from gfx import display, draw' \
    'import gfx.surface' \
    'let canvas = display.open(8, 4, 8, 0)' \
    'draw.rect(canvas, 1, 1, 3, 2, 5, 4)' \
    'print gfx.surface.point(canvas, 2, 2); " "; display.draws()' \
    'print gfx.surface.point(canvas, 3, 2)'

# pixels() AS BYTE: a ubyte list, and to a native a fixed ubyte array, a record's too
check 'quickr pixels' "$(printf '7 7 9 200')" \
    "option dialect quickr" \
    'from gfx import surface' \
    'type Tile' \
    '    px as ubyte[4]' \
    'end type' \
    'let s = surface.new(2, 2)' \
    'var px as ubyte[4]' \
    'px[3] = 7' \
    'surface.write(s, 0, 0, 2, 2, px)' \
    'var back as ubyte[]' \
    'for i in 0..4' \
    '    back.push(0)' \
    'next' \
    'surface.read(s, 0, 0, 2, 2, back)' \
    'back[0] = 9' \
    'back[1] = 200' \
    'surface.write(s, 0, 0, 2, 2, back)' \
    'var t as Tile' \
    'surface.read(s, 0, 0, 2, 2, t.px)' \
    'print back[3]; " "; surface.point(s, 1, 1); " "; t.px[0]; " "; t.px[1]'

# a Quickr module QBasic imports imports gfx
printf '%s\n' "option dialect quickr" 'import gfx.display' 'from gfx import surface, draw' \
    'function paint() as int' '    let c = gfx.display.open(8, 4, 8, 0)' \
    '    draw.rect(c, 1, 1, 3, 2, 5, 4)' '    return surface.point(c, 2, 2)' 'end function' > "$D/qg.bas"
check 'quickr module of qbasic' ' 5 ' \
    'import qg' \
    'print qg.paint()'

check 'no such part' "$(printf 't.bas:1: File not found\n  import gfx.nosuch\n  ^')" \
    'import gfx.nosuch'
echo ok
