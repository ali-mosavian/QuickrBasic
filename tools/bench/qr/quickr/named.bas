' OPTION DIALECT QUICKR
type Named
    x as int
    label as string
    y as int
end type

function walk() as int
    var p = Named(x: 0, label: "p", y: 0)
    for i in 0..10000000
        p.x += 3
        p.y = (p.y + p.x) mod 997
    next
    return p.x + p.y
end function

print walk()
