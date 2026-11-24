' OPTION DIALECT QUICKR
type Vec
    x as int
    y as int
end type

function walk() as int
    var p = Vec(0, 0)
    for i in 0..10000000
        p.x += 3
        p.y = (p.y + p.x) mod 997
    next
    return p.x + p.y
end function

print(walk())
