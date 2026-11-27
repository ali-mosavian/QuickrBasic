option dialect quickr
' a list's room past its length is out of range
var a as list[int]
a.push(1)
a.push(2)
a.push(3)
a.push(4)
a.push(5)
print a.pop(); " "; len(a)
print a[4]
