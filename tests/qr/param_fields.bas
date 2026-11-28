option dialect quickr
' a record parameter's fields read and written through its ref:
' narrow, wide and a held record's
type Inner
    k as short
    d as double
end type
type Rec
    b as byte
    n as int
    inr as Inner
    l as long
    u as ushort
end type

function sum(r as Rec) as int
    return r.b + r.n + r.inr.k + int(r.inr.d) + int(r.l) + r.u
end function

function bump(mut r as Rec, k as int)
    for i in 0..k
        r.n += i
        r.inr.k += 1
        r.inr.d *= 1.5
        r.l += long(2)
        r.u = ushort(r.u + 3)
        r.b = -r.b
    next
end function

var a = Rec(b: 3, n: 1, inr: Inner(k: 2, d: 1.0), l: 5, u: 7)
bump(a, 4)
print sum(a); " "; a.b; " "; a.n; " "; a.inr.k; " "; a.inr.d; " "; a.l; " "; a.u
