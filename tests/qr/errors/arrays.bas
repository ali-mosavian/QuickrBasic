option dialect quickr
' A T[] resizes, a list[T] grows by push(), a T[n] keeps its size.
type Res
    n as int
    fn drop(mut self)
        print "drop"
    end fn
end type
var a as int[]
a.push(1)
var l as list[int]
l.resize(3)
var f as int[3]
f.resize(2)
f.push(1)
var b = int[-1]
var rs as Res[]
rs.resize(2)
var g as int[3, 2]
print g[1]
let s = "ab"
print s[0, 1]
print g.shape(1)
g.resize()
var t as int[,]
