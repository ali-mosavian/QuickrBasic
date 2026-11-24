' OPTION DIALECT QUICKR
' records: defaults, construction by position and name, fields, mut fields, results
type Vec
    x as single
    y as single = 1
end type

type Ball
    r as int = 5
    name as string
    at as Vec
end type

fn length2(v as Vec) as single
    return v.x * v.x + v.y * v.y
end fn

fn scale(mut v as Vec, k as single)
    v.x *= k
    v.y *= k
end fn

fn flip(v as Vec) as Vec
    return Vec(v.y, v.x)
end fn

let v = Vec(3, 4)
print(v.x, v.y, length2(v))
var b = Ball(r: 2, at: v)
print(b.r, b.name, b.at.x, b.at.y)
b.name = "red"
b.at.x = 10
print(b.name, b.at.x, v.x)
var c = b
c.name = "blue"
print(b.name, c.name)
scale(mut b.at, 2)
print(b.at.x, b.at.y)
b.at.scale(0.5)
print(b.at.x, b.at.y, b.at.length2())
var w as Vec
print(w.x, w.y)
w = flip(b.at)
print(w.x, w.y)
w = flip(w)
print(w.x, w.y)
var d as Ball
print(d.r, d.at.y)
