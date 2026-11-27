option dialect quickr
' A T[] of more dimensions: made by T[w, h], read by g[x, y], the first
' subscript the fastest; one subscript reads it flat, as for does. A
' T[n, m] is one inline. shape() gives each dimension's size.
type R
    m as ubyte[3, 2]
end type
type W
    w as int
    m as ubyte[3, 2]
end type
var g as int[]
g = int[3, 2]
g[2, 1] = 7
g[0, 1] = 5
let s = g.shape()
print len(g); " "; len(s); " "; s[0]; " "; s[1]; " "; g[3]; " "; g[5]
var t = 0
for x in g
    t += x
next
print t
g.resize(4, 3)
print len(g); " "; g[2, 1]; " "; g[0, 1]; " "; g[3, 2]
var f as int[2, 2]
f[1, 1] = 9
print len(f); " "; f[1, 1]; " "; f.shape()[1]
var r as R
r.m[2, 1] = 4
r.m[0, 0] = 3
var u = 0
for v in r.m
    u += int(v)
next
print u; " "; r.m.shape()[0]
var e as int[]
print len(e); " "; e.shape()[0]
let b = string[2, 2]
print len(b[1, 1])
' a T[n, m] field past its record's start, in a list's element:
' the first subscript's FX.IDX adds the field's offset, once
var ws as list[W]
ws.push(W(9))
ws[0].m[2, 1] = 6
ws[0].m[0, 1] = 4
print ws[0].w; " "; ws[0].m[2, 1]; " "; ws[0].m[0, 1]; " "; ws[0].m[1, 1]
g[4, 0] = 1
