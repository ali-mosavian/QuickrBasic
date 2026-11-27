option dialect quickr
' A T[] passed whole to a QBasic array parameter: by reference, its length its bounds, a
' REDIM of it the T[]'s
import sums
var a = int[2]
a[0] = 4
a[1] = 5
print sums.total(a); " "; sums.top(a)
sums.grow(a)
print len(a); " "; a[2]
let k = a
print sums.total(k)
var s = short[1]
s[0] = 7
print sums.first(s)
var w = string[2]
w[0] = "x"
w[1] = "y"
print sums.joined(w)
