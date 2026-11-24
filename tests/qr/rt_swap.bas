' OPTION DIALECT QUICKR
' a.swap(i, j): numbers, strings, records with strings, a fixed
' array field and variable; an index past the end out of range
type P
    name as string
    v as int
    fx as short[3]
end type
var a as int[]
var s as string[]
var ps as P[]
for i in 1..5
    a.push(i)
next
s.push("x")
s.push("y")
s.push("z")
ps.push(P(name: "a", v: 1))
ps.push(P(name: "b", v: 2))
a.swap(0, 3)
a.swap(1, 1)
print(a[0], a[1], a[2], a[3])
s.swap(0, 2)
print(s[0], s[1], s[2])
ps.swap(0, 1)
print(ps[0].name, ps[0].v, ps[1].name, ps[1].v)
var p = P(name: "q", v: 0)
p.fx[0] = 5
p.fx[2] = 7
p.fx.swap(0, 2)
print(p.fx[0], p.fx[1], p.fx[2])
var f as int[3]
f[0] = 9
f.swap(0, 1)
print(f[0], f[1])
s[1] = s[0] + s[2]
print(s[1])
a.swap(0, 4)
