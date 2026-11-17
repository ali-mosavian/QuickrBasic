#!/bin/sh
# A file past DOS's 128-char path loads on the host: a fresh clone in a deep directory read
# every program as "File not found" (repl.file was cut at 128).
set -e
B=${1:-build/host/basic}
D=$(mktemp -d)
trap 'rm -rf "${D:?}"' EXIT
P=$D/$(printf 'd%.0s' $(seq 150))
mkdir "$P"
printf 'PRINT 6 * 7\n' > "$P/t.bas"
out=$("$B" "$P/t.bas" < /dev/null | tr -d '\r')
[ "$out" = " 42 " ] || { echo "longpath: got '$out'"; exit 1; }
echo ok
