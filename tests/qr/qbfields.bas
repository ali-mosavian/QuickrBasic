option dialect quickr
' A QBasic TYPE's array fields: a fixed one from 0 a T[n], a dynamic one a T[]
from bland import BLand, fillb, sumb
var b = BLand()
fillb(b)
print b.sea; " "; b.heights[3]; " "; len(b.heights); " "; len(b.g); " "; b.g[2]
b.g.resize(len(b.g) + 1)
b.g[len(b.g) - 1] = 6
b.tags.resize(1)
b.tags[0] = "t"
print sumb(b); " "; b.tags[0]
