option dialect quickr
import math
var f as single[1000]
for i in 0..1000
    f[i] = single(i - 500)
next
var s as single = 0
for k in 0..300
    for i in 0..1000
        s = math.abs(f[i]) + 1
    next
next
print int(s)
