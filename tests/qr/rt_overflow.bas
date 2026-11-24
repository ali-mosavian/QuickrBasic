' OPTION DIALECT QUICKR
' an int's overflow is an error, not a wrap
var n = 2147483647
function grow(mut v as int)
    v += 1
end function
grow(n)
