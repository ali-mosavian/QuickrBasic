' OPTION DIALECT QUICKR
type Pt
    x as int
    y as int
end type

fn total(a as Pt[]) as int
    var s = 0
    for k in 0..4000
        for p in a
            s += p.x + p.y
        end for
    end for
    return s
end fn

var a as Pt[]
for i in 0..1000
    a.push(Pt(i mod 500, i mod 7))
end for
print(total(a))
