' OPTION DIALECT QUICKR
' records in cells: a frame's own in each call, a module's, fields
' at every offset, copies and borrows by address; fields after a
' narrow one; a record whose fields are all strings; narrow fields
' signed and not, a frame's and a module's
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

function bump(mut o as Outer)
    o.k += 1
    o.inr.n *= 2
end function

function deep(n as int) as long
    var o = Outer(tag: 1, k: 0, inr: Inner(b: 2, n: n, d: 0.5), u: 7)
    if n > 0 then
        o.k = deep(n - 1)
    end if
    bump(mut o)
    return o.k + o.inr.n + o.tag + o.inr.b + o.u
end function

var g = Outer(tag: -3, k: 5_000_000_000, inr: Inner(b: -1, n: 9, d: 1.25), u: 4)
bump(mut g)
var h = g
h.inr.d += 1
g.inr.b = 100
print(g.tag, g.k, g.inr.b, g.inr.n, g.inr.d, g.u)
print(h.tag, h.k, h.inr.b, h.inr.n, h.inr.d, h.u)
print(deep(3))
function halves(n as int) as (int, double)
    return (n, n / 2)
end function

var s = 0.0
for i in 0..4
    let a, b = halves(i)
    s += a + b
next
print(s)
var m as int? = none
print(m ?? -1)
m = 6
print(m ?? -1)

type Odd
    b as byte
    n as int
    s as short
    d as double
    l as long
end type

type Names
    a as string
    b as string
end type

function walk(k as int) as int
    var o = Odd(b: 1, n: 2, s: 3, d: 4.5, l: 6)
    for i in 0..k
        o.n += i
        o.d *= 1.0
        o.l += long(o.n)
        o.b = 7
    next
    return o.n + o.b + o.s + int(o.l)
end function

var od = Odd(b: 1, n: 2, s: 3, d: 4.5, l: 6)
od.n += 40
od.l -= long(1)
let nm = Names(a: "x", b: "y")
print(walk(10))
print(od.b, od.n, od.s, od.d, od.l)
print(nm.a + nm.b)

type Narrow
    b as byte
    u as ubyte
    s as short
    w as ushort
    n as int
end type

function narrow(k as int) as int
    var p = Narrow(b: -1, u: 200, s: -300, w: 60000, n: 5)
    for i in 0..k
        p.s += 1
        p.b = -p.b
        p.n += p.u + p.w
    next
    return p.n + p.s + p.b
end function

var nw = Narrow(b: 1, u: 2, s: 3, w: 4, n: 0)
nw.s -= 10
nw.u += 250
print(narrow(10), nw.b, nw.u, nw.s, nw.w)
