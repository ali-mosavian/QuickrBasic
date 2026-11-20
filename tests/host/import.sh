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
printf 'import deep.libx\nimport deep.liby\next1\nown1\n' > "$D/prog/t.bas"
cp "$B" "$D/bin/basic"
ln -s ../bin/basic "$D/prog/basic"
long=$D/$(printf 'n%.0s' $(seq 300))
out=$(cd "$D/prog" && BASICLIB="$long;$D/ext" ./basic t.bas < /dev/null | tr -d '\r')
[ "$out" = "$(printf 'ext1\nown1')" ] || { echo "import: got '$out'"; exit 1; }
echo ok
