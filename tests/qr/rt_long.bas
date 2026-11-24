' OPTION DIALECT QUICKR
' a long's overflow is an error, not a wrap
var l as long = 9_000_000_000_000_000_000
print l + 1
print l * 2
