' OPTION DIALECT QUICKR
var s = 0
for i in 0..30000000
    s += i mod 7
next
print(s)
