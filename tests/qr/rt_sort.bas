option dialect quickr
' a.sort(less): records by a field, stable, by a block lambda
' and a function; a.sort() by value: ints, uints, longs, doubles,
' strings, a fixed array field; a compare's error raised
type P
    name as string
    v as int
    fx as short[4]
end type
function shorter(x as string, y as string) as bool
    return len(x) < len(y)
end function
var ps as P[]
ps.push(P(name: "c", v: 1))
ps.push(P(name: "a", v: 2))
ps.push(P(name: "e", v: 1))
ps.push(P(name: "b", v: 1))
ps.push(P(name: "d", v: 0))
ps.sort(fn(x, y) x.name < y.name)
ps.sort(fn(x, y) x.v < y.v)
for p in ps
    print p.name; " "; p.v
next
var s as string[]
s.push("ccc")
s.push("b")
s.push("aa")
s.push("dd")
s.sort(shorter)
print s[0]; " "; s[1]; " "; s[2]; " "; s[3]
var by = 1
s.sort(fn(x, y)
    var k = by
    return x[k - 1] > y[k - 1]
end fn)
print s[0]; " "; s[1]; " "; s[2]; " "; s[3]
s.sort()
print s[0]; " "; s[1]; " "; s[2]; " "; s[3]
var a as int[]
for k in 0..10
    a.push((k * 7) mod 10 - 5)
next
a.sort()
print a[0]; " "; a[1]; " "; a[5]; " "; a[9]
var u as uint[]
u.push(uint(3000000000))
u.push(uint(1))
u.push(uint(2147483648))
u.sort()
print u[0]; " "; u[1]; " "; u[2]
var l as long[]
l.push(long(5000000000))
l.push(long(-5000000000))
l.push(long(0))
l.sort()
print l[0]; " "; l[1]; " "; l[2]
var d as double[]
d.push(2.5)
d.push(-1.0)
d.push(0.25)
d.sort()
print d[0]; " "; d[1]; " "; d[2]
var p = P(name: "q")
p.fx[0] = 5
p.fx[1] = -3
p.fx[2] = 7
p.fx[3] = 1
p.fx.sort()
print p.fx[0]; " "; p.fx[1]; " "; p.fx[2]; " "; p.fx[3]
p.fx.sort(fn(x, y) x > y)
print p.fx[0]; " "; p.fx[1]; " "; p.fx[2]; " "; p.fx[3]
a.sort(fn(x, y) 1 / x < 1 / y)
