' OPTION DIALECT QUICKR
' A QBasic TYPE's array fields: a fixed one from 0 a fixed array, a dynamic one a list
from bland import BLand, fillb, sumb
var b = BLand()
fillb(b)
print b.sea; " "; b.heights[3]; " "; len(b.heights); " "; len(b.g); " "; b.g[2]
b.g.push(6)
b.tags.push("t")
print sumb(b); " "; b.tags[0]
