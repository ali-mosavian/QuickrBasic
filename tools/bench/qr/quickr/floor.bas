option dialect quickr
import math
var x = 0.0
var ix = 0
for k in 0..300000
    x += 0.37
    ix = int(math.floor(x))
next
print ix
