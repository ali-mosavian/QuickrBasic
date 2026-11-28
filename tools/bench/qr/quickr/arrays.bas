option dialect quickr
var a as int[1000]
for i in 0..1000
    a[i] = i * 3
next
var s = 0
for k in 0..3000
    for x in a
        s += x mod 7
    next
next
print s
