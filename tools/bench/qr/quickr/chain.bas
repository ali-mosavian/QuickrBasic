option dialect quickr
var a as int[1000]
for i in 0..1000
    a[i] = (i * 7919) mod 1000
next
var t = 0
for r in 0..300
    t += a.filter(fn(x) x mod 3 = 0).map(fn(x) x * 2).sum()
next
print t
