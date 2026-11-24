' OPTION DIALECT QUICKR
' for x in a copies each element E.NEXT loads: narrow ints, enums,
' records of numbers, field words; a kept generator's loop too
enum Dir
    north
    south
end enum

type Pt
    tag as byte
    x as int
    y as double
end type

type Two
    a as Pt
    n as short
end type

function of(xs as long[]) yields long
    for x in xs
        yield x * long(2)
    next
end function

function points(ps as Pt[]) yields int
    for p in ps
        yield p.x + p.tag
    next
end function

var bs as byte[]
var us as ushort[]
for i in 0..5
    bs.push(byte(i - 2))
    us.push(ushort(60000 + i))
next
var t = 0
for b in bs
    t += b
next
var u = 0
for w in us
    u += w - 60000
next
print(t, u)
var ds as Dir[]
ds.push(Dir.south)
ds.push(Dir.north)
for d in ds
    print(d = Dir.south)
next
var ps as Pt[]
var twos as Two[]
for i in 0..4
    ps.push(Pt(tag: byte(i), x: i * 10, y: 0.5))
    twos.push(Two(a: Pt(tag: byte(1), x: i, y: 1.0), n: short(i)))
next
var s = 0.0
for p in ps
    s += double(p.x + p.tag) + p.y
next
var n = 0
for tw in twos
    n += tw.a.x + tw.n + tw.a.tag
next
print(s, n)
var ls as long[]
ls.push(long(5))
ls.push(long(7))
var it = of(ls)
var lt = long(0)
for x in it
    lt += x
next
var pi = points(ps)
var pt = 0
for v in pi
    pt += v
next
print(lt, pt)
