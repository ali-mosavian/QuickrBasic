' OPTION DIALECT QUICKR
' records in cells: a frame's own in each call, a module's, fields
' at every offset, copies and borrows by address
type Inner
    n as int
    b as byte
    d as double
end type

type Outer
    k as long
    u as uint
    inr as Inner
    tag as short
end type

fn bump(mut o as Outer)
    o.k += 1
    o.inr.n *= 2
end fn

fn deep(n as int) as long
    var o = Outer(tag: 1, k: 0, inr: Inner(b: 2, n: n, d: 0.5), u: 7)
    if n > 0 then
        o.k = deep(n - 1)
    end if
    bump(mut o)
    return o.k + o.inr.n + o.tag + o.inr.b + o.u
end fn

var g = Outer(tag: -3, k: 5_000_000_000, inr: Inner(b: -1, n: 9, d: 1.25), u: 4)
bump(mut g)
var h = g
h.inr.d += 1
g.inr.b = 100
print(g.tag, g.k, g.inr.b, g.inr.n, g.inr.d, g.u)
print(h.tag, h.k, h.inr.b, h.inr.n, h.inr.d, h.u)
print(deep(3))
fn halves(n as int) as (int, double)
    return (n, n / 2)
end fn

var s = 0.0
for i in 0..4
    let a, b = halves(i)
    s += a + b
end for
print(s)
var m as int? = none
print(m ?? -1)
m = 6
print(m ?? -1)
