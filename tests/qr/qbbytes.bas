option dialect quickr
' A QBasic BYTE array is a ubyte[].
import pal
var a = ubyte[3]
pal.ramp(a)
print a[0]; " "; a[2]
