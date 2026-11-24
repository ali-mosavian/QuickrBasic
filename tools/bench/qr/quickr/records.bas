' OPTION DIALECT QUICKR
type Vec
    x as int
    y as int
end type

function advance(mut p as Vec, v as Vec)
    p.x += v.x
    p.y += v.y
end function

var p = Vec(0, 0)
let v = Vec(3, 1)
for i in 0..2000000
    advance(mut p, v)
    p.x = p.x mod 1000
end for
print(p.x, p.y)
