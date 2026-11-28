option dialect quickr
' for x in a: x a copy each step loads, a list a parameter's, a
' global's or a local's; the copy read as a[i] is
var g as list[double]
g.push(1.5)
g.push(-2.25)
g.push(4.0)

function total(a as list[long]) as long
    var s as long = 0
    for x in a
        s += x * x
    next
    return s
end function

function gsum() as double
    var s = 0.0
    for x in g
        for y in g
            s += x * y
        next
    next
    return s
end function

function apply(f as function(int) as int, x as int) as int
    return f(x)
end function

var none_ as list[int]
var n = 0
for x in none_
    n += 1
next
print n
var ls as list[long]
ls.push(3_000_000_000)
ls.push(-4)
print total(ls); " "; gsum()
var bs as list[bool]
bs.push(true)
bs.push(false)
bs.push(true)
var t = 0
for i, b in bs
    if not b then continue
    t += i + apply(fn(k) k * 10, i)
next
print t
var fs as list[single]
fs.push(0.5)
fs.push(0.25)
var u as list[uint]
u.push(4_000_000_000)
for x in u
    print x
next
for i, x in fs
    print i; " "; x; " "; fs[i]
    if i = 0 then break
next

function doubled(a as list[int]) yields int
    for x in a
        yield x * 2
    next
end function

var w as list[int]
w.push(5)
w.push(6)
w.push(7)
var kept = doubled(w)
print kept.next() or else -1; " "; kept.next() or else -1; " "; kept.next() or else -1; " "; kept.next() or else -1
