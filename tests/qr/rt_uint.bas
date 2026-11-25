' OPTION DIALECT QUICKR
' a uint past 2^32 - 1 is an Overflow
var u as uint = 4_000_000_000
let v as uint = 294_967_295
print u + v
print u + v + 1
