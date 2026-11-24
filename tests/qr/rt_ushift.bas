' OPTION DIALECT QUICKR
' a ulong shifts by 0 to 63, no further
var u as ulong = 1
var n = 63
print(u shl n)
n += 1
print(u shl n)
