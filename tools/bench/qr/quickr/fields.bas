' OPTION DIALECT QUICKR
type Vec
    x as int
    y as int
end type

fn walk() as int
    var p = Vec(0, 0)
    for i in 0..10000000
        p.x += 3
        p.y = (p.y + p.x) mod 997
    end for
    return p.x + p.y
end fn

print(walk())
