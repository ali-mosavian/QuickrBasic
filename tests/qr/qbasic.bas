option dialect quickr
' A QBasic module's SUBs, FUNCTIONs and TYPEs: numbers, strings and records each way, by
' reference into a var, as a copy from anything else
import twice
import shapes
from shapes import area, Rect, Pt, SIDES, TITLE
print "main"
print twice.quad(3); " "; shapes.total()
var r = Rect(2, 3, "r")
print area(r); " "; shapes.AREA(r)
shapes.grow(r, 1)
print r.w; " "; r.h; " "; r.label
let lr = Rect(1, 1, "let")
shapes.grow(lr, 5)
print lr.w; " "; lr.label
var p = Pt(1, 2, 3)
shapes.move(p, 10)
print p.x; " "; p.y; " "; p.s
let lp = Pt(1, 2, 3)
shapes.move(lp, 10)
print lp.x
var n as int = 5
var l as int = 10
var s as short = 4
shapes.bump(n, l, s)
print n; " "; l; " "; s
let k as int = 5
shapes.bump(k, k + 1, 3)
print k
var who = "bob"
print shapes.greet(who); " "; who
print shapes.greet("ann")
print shapes.half(5)
let q = shapes.square(4)
print q.w; " "; q.h; " "; q.label
' its CONSTs: whole numbers consts, the rest read as its code set them
const MORE = SIDES + 1
print SIDES; " "; shapes.DIAGS; " "; MORE; " "; shapes.SHARE; " "; shapes.TAU; " "; TITLE
