option dialect quickr
' A list passed whole to a QBasic array parameter: by reference, its length its bounds, a
' REDIM of it the list's
import sums
var a as int[]
a.push(4)
a.push(5)
print sums.total(a); " "; sums.top(a)
sums.grow(a)
print len(a); " "; a[2]
let k = a
print sums.total(k)
var s as short[]
s.push(7)
print sums.first(s)
var w as string[]
w.push("x")
w.push("y")
print sums.joined(w)
