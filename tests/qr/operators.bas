' OPTION DIALECT QUICKR
' function operator: + - * / = < on records, inlined when one return; one
' an operator and pair of types, the record on either side; unary -
from shapes import Size
import shapes
type Vec
    x as int
    y as int
end type

type Name
    first as string
    last as string
end type

function operator +(a as Vec, b as Vec) as Vec
    return Vec(a.x + b.x, a.y + b.y)
end function

function operator -(a as Vec, b as Vec) as Vec
    return Vec(a.x - b.x, a.y - b.y)
end function

function operator *(a as Vec, k as int) as Vec
    return Vec(a.x * k, a.y * k)
end function

function operator *(k as int, a as Vec) as Vec
    return a * k
end function

function operator *(a as Vec, b as Vec) as int
    return a.x * b.x + a.y * b.y
end function

function operator -(a as Vec) as Vec
    return Vec(-a.x, -a.y)
end function

function operator -(n as Name) as Name
    let r = Name(n.last, n.first)
    return r
end function

function operator /(a as Vec, k as int) as Vec
    let r = Vec(a.x div k, a.y div k)
    return r
end function

function operator =(a as Vec, b as Vec) as bool
    return a.x = b.x and a.y = b.y
end function

function operator <(a as Vec, b as Vec) as bool
    if a.x <> b.x then return a.x < b.x
    return a.y < b.y
end function


function operator +(a as Name, b as Name) as Name
    return Name(a.first + b.first, a.last + b.last)
end function

function largest[T](a as T[]) as T
    var best = a[0]
    for x in a
        if x > best then best = x
    next
    return best
end function

type Box
    at as Vec
    size as Vec
end type

var p = Vec(1, 2)
let v = Vec(3, 4)
p = p + v
print p.x; " "; p.y
p = Vec(p.y, p.x)
print p.x; " "; p.y
p = Vec(p.y, p.x) - v * 2
print p.x; " "; p.y
p += v
p *= 3
print p.x; " "; p.y
let q = (p + v) * 3 + p
print q.x; " "; q.y; " "; (q - p).x; " "; (q / 2).y
print p = p; " "; p <> v; " "; p < v; " "; p > v; " "; p <= p; " "; p >= v
var vs as Vec[]
vs.push(Vec(1, 9))
vs.push(Vec(4, 0))
vs.push(Vec(4, 2))
let big = largest(vs)
print big.x; " "; big.y
vs[1] += Vec(10, 10)
print vs[1].x; " "; vs[1].y
var b = Box(at: Vec(1, 1), size: Vec(2, 3))
b.at = b.at + b.size
b.size = b.at - b.size
print b.at.x; " "; b.at.y; " "; b.size.x; " "; b.size.y
let n = Name("a", "b") + Name("c", "d")
print n.first; " "; n.last
let s = Size(5, 6) + Size(1, 2)
print s.w; " "; s.h; " "; shapes.shrink(s).w
let w = 2 * v
print w.x; " "; w.y; " "; v * w; " "; (-v).x; " "; (-(v * 2) + v).y; " "; (-n).first
