' A QBasic program passes its arrays to Quickr functions taking T[]s:
' by reference, a mut one's growth and shrinking seen; a fixed array
' field's and one of two dimensions too. One not from 0 is no T[].
IMPORT foes
TYPE Row
    V(2) AS LONG
END TYPE
DIM xs(2) AS LONG
xs(0) = 1: xs(1) = 2: xs(2) = 4
PRINT foes.total(xs())
foes.shrink xs()
PRINT UBOUND(xs); foes.total(xs())
DIM f(1) AS foes.Foe
f(0).hp = 10: f(0).tag = "orc"
f(1).hp = 7: f(1).tag = "elf"
foes.hurt f(), 3
PRINT UBOUND(f); f(0).hp; f(1).hp; f(2).hp; f(2).tag
PRINT foes.names(f())
DIM r AS Row
r.V(1) = 5: r.V(2) = 6
PRINT foes.total(r.V())
DIM g(2, 2) AS LONG
g(2, 1) = 3
PRINT foes.total(g())
DIM h(1 TO 3) AS LONG
PRINT foes.total(h())
