#!/bin/sh
# A DOS name in any case finds the file on a case-sensitive disk: on Linux '$INCLUDE: 'q45i33.inc'
# missed Q45I33.INC with "File not found".
set -e
B=$(cd "$(dirname "${1:-build/host/basic}")" && pwd)/$(basename "${1:-build/host/basic}")
D=$(mktemp -d "${TMPDIR:-/tmp}/dosname.XXXXXX")
trap 'rm -rf "${D:?}"' EXIT
mkdir "$D/Sub"
printf 'PRINT 6 * 7\n' > "$D/Sub/INC.BI"
printf '7\n' > "$D/Sub/Data.Txt"
printf "' \$INCLUDE: 'sub\\\\inc.bi'\nOPEN \"SUB\\\\DATA.TXT\" FOR INPUT AS 1: INPUT #1, n: PRINT n\n" > "$D/T.BAS"
out=$(cd "$D" && "$B" t.bas < /dev/null | tr -d '\r')
[ "$out" = "$(printf ' 42 \n 7 ')" ] || { echo "dosname: got '$out'"; exit 1; }
echo ok
