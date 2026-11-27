option dialect quickr
' a QBasic fs() as F takes a Quickr F[n] and F[]; F's elements
' were read as singles: "pass a var of its type, a single[]"
from qb import F, live
var fs as F[3]
fs[0].hp = 4
fs[2].hp = 1
print live(fs)
var d as F[] = F[2]
d[1].hp = 9
print live(d)
