option dialect quickr
' A T[] is an array: made n long by T[n], n a run-time value, its size changed by resize();
' a list[T] grows by push().
type P
    x as int
    s as string
end type
type Grid
    cells as int[]
end type
var a as int[]
print len(a)
let n = 3
a = int[n + 2]
a[4] = 7
print len(a); " "; a[4]
a.resize(8)
print len(a); " "; a[4]; " "; a[7]
a.resize(2)
a.resize(5)
print len(a); " "; a[4]
var ps = P[2]
ps[1] = P(x: 3, s: "rec")
ps.resize(4)
print len(ps); " "; ps[1].s; " "; ps[3].s = ""
ps.resize(1)
print len(ps)
var g = Grid(cells: int[4])
g.cells[2] = 9
let b = g.cells
g.cells[2] = 1
print b[2]; " "; g.cells[2]; " "; len(g.cells[1..3])
var l as list[string]
l.push("x")
l.push("y")
print len(l); " "; l[1]
var t = 0
for x in int[3]
    t += 1
next
print t
a.resize(-1)
