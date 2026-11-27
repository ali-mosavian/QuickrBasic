' OPTION DIALECT QUICKR
' A QBasic BYTE array is a ubyte list.
import pal
var a as ubyte[]
for i in 0..3
    a.push(0)
next
pal.ramp(a)
print a[0]; " "; a[2]
