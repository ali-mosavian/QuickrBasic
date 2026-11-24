' OPTION DIALECT QUICKR
var a as int[1000]
for i in 0..1000
    a[i] = (i * 7919) mod 1000
end for
var t = 0
for r in 0..300
    t += a.filter(fn(x) x mod 3 = 0).map(fn(x) x * 2).sum()
end for
print(t)
