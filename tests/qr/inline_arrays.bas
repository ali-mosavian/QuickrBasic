option dialect quickr
' a fixed array inline, a record's or a list's element: assigned and passed whole
type R
    v as int[3]
    names as string[2]
end type
function sum(a as int[3]) as int
    var t = 0
    for x in a
        t += x
    next
    return t
end function
function set(mut a as int[3])
    a[1] = 40
end function
var a as int[3]
a[2] = 5
var r as R
r.v = a
print r.v[2]; " "; sum(r.v)
set(r.v)
print r.v[1]; " "; a[1]
var b = r.v
b[0] = 7
print b[0]; " "; r.v[0]
var s as string[2]
s[1] = "kept"
r.names = s
s[1] = "changed"
print r.names[1]
var pairs as list[int[3]]
pairs.push(a)
pairs.push(r.v)
pairs[0][0] = 9
print pairs[0][0]; " "; a[0]; " "; sum(pairs[1]); " "; len(pairs)
