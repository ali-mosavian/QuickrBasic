' OPTION DIALECT QUICKR
' math: sqrt .. floor the VM's ops; generics and Vec in Quickr
import math
from math import Vec, sqrt
print(math.sqrt(16), math.floor(2.7), math.floor(-2.5), math.ceil(2.1), math.round(2.5))
print(math.sin(0), math.cos(0), math.exp(0), math.log(1), math.tan(0))
print(math.atan2(1, 1) * 4 = math.pi, math.atan2(1, -1) > 2, math.atan2(-1, 0) < 0)
print(math.abs(-3), math.abs(-2.5), math.min(3, 4), math.max(2.5, 1.0))
print(math.clamp(15, 0, 10), math.clamp(-0.5, 0.0, 1.0), math.lerp(0, 10, 0.25))
let v = Vec(3, 4)
print(v.length(), (v + v).x, (v * 2).y, v.dot(v), (v - Vec(1, 1)).x, (v / 2).x)
print(v = Vec(3, 4), v <> v * 1, v + v = v * 2)
print(sqrt(2))
