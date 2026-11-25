' OPTION DIALECT QUICKR
' assert in a program: a false one stops it with its line
let x = 3
assert x = 3
print "ok"
if x > 1 then assert x < 2
print "not here"
