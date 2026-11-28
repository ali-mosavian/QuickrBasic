option dialect quickr
' A T[] parameter takes any array of its elements by its descriptor,
' QBasic's: a T[], a T[n] or a T[n, m], a var's or a record's. A mut one
' changes the elements; a fixed array's size it can't.
type R
    m as int[3, 2]
end type
function total(a as int[]) as int
    var t = 0
    for x in a
        t += x
    next
    return t
end function
function fill(mut a as int[], v as int)
    for i in 0..len(a)
        a[i] = v
    next
end function
function corner(mut a as int[]) as int
    a[2, 1] = 9
    return a.shape()[1]
end function
function grow(mut a as int[])
    a.resize(len(a) + 1)
end function
var f as int[4]
fill(f, 2)
print total(f)
var r as R
fill(r.m, 1)
print total(r.m); " "; corner(r.m); " "; r.m[2, 1]
var d = int[3]
fill(d, 5)
grow(d)
print total(d); " "; len(d)
var q as int[3, 2]
print corner(q); " "; q[2, 1]
grow(f)
