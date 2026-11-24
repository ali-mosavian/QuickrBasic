' OPTION DIALECT QUICKR
' a long added to in place overflows as a + b does
var t as long = 9_223_372_036_854_775_806
var i = 1
t += i
print(t)
t += i
print(t)
