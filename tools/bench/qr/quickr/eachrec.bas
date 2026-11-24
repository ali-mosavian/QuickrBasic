' OPTION DIALECT QUICKR
type Pt
    x as int
    y as int
end type

function total(a as Pt[]) as int
    var s = 0
    for k in 0..4000
        for p in a
            s += p.x + p.y
        next
    next
    return s
end function

var a as Pt[]
for i in 0..1000
    a.push(Pt(i mod 500, i mod 7))
next
print(total(a))
