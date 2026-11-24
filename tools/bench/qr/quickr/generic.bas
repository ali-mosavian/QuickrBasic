' OPTION DIALECT QUICKR
fn largest[T](a as T[1000]) as T
    var best = a[0]
    for x in a
        if x > best then best = x
    end for
    return best
end fn
var a as int[1000]
for i in 0..1000
    a[i] = (i * 7919) mod 1000
end for
var t = 0
for r in 0..300
    t += largest(a)
end for
print(t)
