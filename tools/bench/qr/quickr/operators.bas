' OPTION DIALECT QUICKR
type Vec
    x as int
    y as int
end type

function operator +(a as Vec, b as Vec) as Vec
    return Vec(a.x + b.x, a.y + b.y)
end function

var p = Vec(0, 0)
let v = Vec(3, 1)
for i in 0..2000000
    p = p + v
    p.x = p.x mod 1000
next
print p.x; " "; p.y
