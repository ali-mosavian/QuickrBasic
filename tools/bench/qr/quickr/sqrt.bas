option dialect quickr
import math
var t = 0.0
for i in 0..200000
    t += math.sqrt(double(i))
next
print int(t)
