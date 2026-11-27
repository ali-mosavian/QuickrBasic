option dialect quickr
' math's generics, round, ceil, atan2 and lerp, each a VM op: the
' values their Quickr bodies gave, for each kind of number
import math
let b = byte(-5)
let h = short(-300)
let l = long(-5000000000)
let u = uint(7)
let w = ulong(9)
print math.abs(b); " "; math.abs(h); " "; math.abs(-7); " "; math.abs(l)
print math.abs(u); " "; math.abs(w); " "; math.abs(single(-1.5)); " "; math.abs(-0.25)
print math.abs(-0.0); " "; math.abs(0.0)
print math.min(b, byte(3)); " "; math.max(h, short(2)); " "; math.min(l, long(1))
print math.max(u, uint(4)); " "; math.min(w, ulong(2)); " "; math.max(single(1.5), single(-2))
print math.clamp(-3, 0, 10); " "; math.clamp(12, 0, 10); " "; math.clamp(5, 0, 10)
print math.clamp(5, 10, 0); " "; math.clamp(single(0.5), single(0), single(1))
print math.clamp(u, uint(8), uint(9)); " "; math.clamp(2.5, 0.0, 1.0)
print math.round(2.5); " "; math.round(-2.5); " "; math.round(-0.2); " "; math.round(7.0)
print math.ceil(2.1); " "; math.ceil(-0.5); " "; math.ceil(-2.0); " "; math.ceil(3.0)
print math.atan2(1, 1); " "; math.atan2(1, -1); " "; math.atan2(-1, -1); " "; math.atan2(-1, 1)
print math.atan2(1, 0); " "; math.atan2(-1, 0); " "; math.atan2(0, 0); " "; math.atan2(0, -1)
print math.lerp(0, 10, 0.25); " "; math.lerp(10, 0, 0.25); " "; math.lerp(-1, 1, 2)
print math.abs(byte(-128))
