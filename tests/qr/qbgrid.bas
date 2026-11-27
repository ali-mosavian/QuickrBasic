option dialect quickr
' Arrays of two dimensions across: a T[], a T[n, m] and a QBasic TYPE's
' fixed array field pass to QBasic's a() by their descriptors, each
' subscript where QBasic has it.
import grid
var g = int[3, 2]
grid.mark(g)
print g[2, 1]; " "; grid.total(g)
var m as grid.Map
m.c[1, 0] = 4
grid.mark(m.c)
print m.c[2, 1]; " "; grid.total(m.c); " "; m.c.shape()[1]
var f as int[3, 2]
f[0, 0] = 1
grid.mark(f)
print grid.total(f)
