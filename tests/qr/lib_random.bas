option dialect quickr
' random: int(a, b) within its ends, each value met; real(a, b) below b
import random
random.seed(7)
var seen as bool[6]
var low = 1.0
for i in 0..2000
    let n = random.int(1, 6)
    seen[n - 1] = true
    let r = random.real(-2, 2)
    if r < -2 or r >= 2 then print "out"; " "; r
    if r < low then low = r
next
print seen.all(fn(x) x); " "; low < -1.9
var xs as string[]
xs.push("a")
xs.push("b")
print random.pick(xs) in xs
