option dialect quickr
var f as single[1000]
for i in 0..1000
    f[i] = single((i * 37) mod 101)
next
var lo = f[0]
var hi = f[0]
for k in 0..300
    for i in 1..1000
        if f[i] < lo then lo = f[i]
        if f[i] > hi then hi = f[i]
    next
next
print int(lo)
print int(hi)
