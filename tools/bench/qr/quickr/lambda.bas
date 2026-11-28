option dialect quickr
function sum_where(a as int[1000], keep as function(int) as bool) as int
    var t = 0
    for x in a
        if keep(x) then t += x
    next
    return t
end function
var a as int[1000]
for i in 0..1000
    a[i] = (i * 7919) mod 1000
next
var t = 0
let lim = 500
for r in 0..300
    t += sum_where(a, fn(x) x > lim)
next
print t
