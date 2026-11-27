' OPTION DIALECT QUICKR
' A QBasic TYPE of BYTE fields, an array, a dynamic one and one
' alone, is a record of ubyte fields.
from bl import BL, fill
var x as BL
fill(x)
print x.a[2]; " "; x.one; " "; x.sea; " "; x.g[2]; " "; len(x.g)
