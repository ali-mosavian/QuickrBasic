option dialect quickr
type Narrow
    s as short
    t as short
end type

function walk() as int
    var p = Narrow(0, 0)
    for i in 0..10000000
        p.s = (p.s + 7) mod 1001
        p.t = (p.t + p.s) mod 997
    next
    return int(p.s) + int(p.t)
end function

print walk()
