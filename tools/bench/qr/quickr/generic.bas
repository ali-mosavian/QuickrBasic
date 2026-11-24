' OPTION DIALECT QUICKR
function largest[T](a as T[1000]) as T
    var best = a[0]
    for x in a
        if x > best then best = x
    next
    return best
end function
var a as int[1000]
for i in 0..1000
    a[i] = (i * 7919) mod 1000
next
var t = 0
for r in 0..300
    t += largest(a)
next
print(t)
