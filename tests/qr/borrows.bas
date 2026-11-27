option dialect quickr
' a mut field and its record's other fields lent in one call:
' disjoint places. It was "g is lent mut in this call"
type P
    x as int
end type
type Two
    a as P
    b as P
end type
type G
    t as Two
    c as P
end type
function f(mut a as P, b as P)
    a.x = b.x + 1
end function
function both(mut a as P, mut b as P)
    a.x = 1
    b.x = 2
end function
var g as G
g.t.b.x = 4
f(g.t.a, g.t.b)
print g.t.a.x
f(g.c, g.t.a)
print g.c.x
both(g.t.a, g.c)
print g.t.a.x; " "; g.c.x
