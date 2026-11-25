#!/bin/sh
# Modules past the program's folder: in BASICLIB's folders, then in lib/ beside the interpreter,
# run through a link too. The host's BASIC saw no BASICLIB, then dropped one longer than the
# environment's first buffer; through a link it looked for lib/ beside the link: File not found.
set -e
B=$(cd "$(dirname "${1:-build/host/basic}")" && pwd)/$(basename "${1:-build/host/basic}")
D=$(mktemp -d "${TMPDIR:-/tmp}/import.XXXXXX")
trap 'rm -rf "${D:?}"' EXIT
mkdir -p "$D/ext/deep" "$D/bin/lib/deep" "$D/prog"
printf 'sub ext1\n print "ext1"\nend sub\n' > "$D/ext/deep/libx.bas"
printf 'sub own1\n print "own1"\nend sub\n' > "$D/bin/lib/deep/liby.bas"
printf 'from deep.libx import ext1\nimport deep.liby\next1\ndeep.liby.own1\n' > "$D/prog/t.bas"
cp "$B" "$D/bin/basic"
ln -s ../bin/basic "$D/prog/basic"
long=$D/$(printf 'n%.0s' $(seq 300))
out=$(cd "$D/prog" && BASICLIB="$long;$D/ext" ./basic t.bas < /dev/null | tr -d '\r')
[ "$out" = "$(printf 'ext1\nown1')" ] || { echo "import: got '$out'"; exit 1; }

# A FUNCTION of another module whose result is a TYPE: its own module held its result's type, the
# caller's looked in its own and found none: Type mismatch.
printf 'type p\n x as integer\nend type\nfunction mk(v) as p\n dim r as p\n r.x = v\n mk = r\nend function\n' > "$D/prog/rec.bas"
printf 'import rec\nfrom rec import mk, p\ndim a as p\na = rec.mk(3)\nprint a.x\na = mk(4)\nprint a.x\n' > "$D/prog/r.bas"
out=$(cd "$D/prog" && ./basic r.bas < /dev/null | tr -d '\r')
[ "$out" = "$(printf ' 3 \n 4 ')" ] || { echo "import: a TYPE result got '$out'"; exit 1; }
echo ok
