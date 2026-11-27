option dialect quickr
' assignment and compound assignment
var a = 10
a = a + 5
print a
a += 2
a -= 4
a *= 3
print a
var d as double = 9
d /= 2
print d
var s = "ab"
s += "cd"
s = s + "!"
print s
var x as single = 1
x += 0.5
x *= 2
print x
var b = false
b = not b
print b
' a compound's target's subscripts evaluated once: once read
' twice, a[next()] += 10 added a[1] + 10 into a[2]
var calls = 0
function next() as int
    calls += 1
    return calls
end function
var c as int[4]
c[next()] += 10
print c[1]; " "; c[2]; " "; calls
var i = 0
function bump() as int
    i += 1
    return 5
end function
c[i] += bump()
print c[0]; " "; c[1]; " "; i
