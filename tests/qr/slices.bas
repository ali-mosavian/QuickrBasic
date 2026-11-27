option dialect quickr
' strings' bytes and slices, lists' slices
let s = "hello, world"
print s[0]; " "; s[4]; " "; len(s)
print s[0..5]; " "; "|"; " "; s[7..]; " "; "|"; " "; s[..5]; " "; "|"; " "; s[3..3]; " "; "|"; " "; s[8..2]; " "; "|"; " "; s[-4..100]
var t = s[7..]
t = t[1..4]
print t
var n = 0
for i in 0..len(s)
    if s[i] = 111 then
        n += 1
    end if
next
print n
type P
    name as string
    tags as string[]
end type
var a as int[]
for i in 0..10
    a.push(i * i)
next
let b = a[2..5]
print len(b); " "; b[0]; " "; b[2]
var c = a[..3]
c.push(7)
print len(c); " "; c[3]; " "; len(a); " "; len(a[8..]); " "; len(a[20..])
a = a[5..]
print len(a); " "; a[0]
var ps as P[]
ps.push(P("x"))
ps[0].tags.push("t1")
ps[0].tags.push("t2")
ps.push(P("y"))
var qs = ps[1..]
qs[0].name = "z"
let rs = ps[..1]
print len(qs); " "; qs[0].name; " "; ps[1].name; " "; rs[0].tags[1]; " "; len(rs[0].tags)
print s[5]
