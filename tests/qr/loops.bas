option dialect quickr
' while, for to b and b included, for over a range (not including its end), do; break and continue
var i = 0
while i < 3
    print "w"; " "; i
    i += 1
wend
for k in 0..3
    print "f"; " "; k
next
for k = 1 to 10 step 4
    print "s"; " "; k
next
for k = 5 to 0 step -2
    print "d"; " "; k
next
for k in 3..3
    print "never"
next
var n = 0
do
    n += 1
    if n = 2 then continue
    if n > 4 then break
    print "do"; " "; n
loop
for k in 0..10
    if k mod 2 = 1 then continue
    if k > 6 then break
    print "e"; " "; k
next
var j = 0
while true
    j += 1
    if j < 3 then continue
    break
wend
print j
let lo = 2
let hi = 5
for k in lo..hi
    for m in 0..k
        if m = 2 then break
        print k; " "; m
    next
next
function sum_to(n as int) as int
    var t = 0
    for k = 0 to n
        t += k
    next
    return t
end function
print sum_to(100)
var up = 2
var dn = -2
for k = 0 to 6 step up
    print "vu"; " "; k
next
for k = 4 to 0 step dn
    print "vd"; " "; k
next
for k = 3 to 2 step up
    print "never"
next
for k in 2147483645..2147483647
    print "top"; " "; k
next
for k = -2147483646 to -2147483647 step -1
    print "bottom"; " "; k
next
let low = -2147483648
for k in 0..low
    print "never"
next
function count_in(n as int, by as int) as int
    var t = 0
    for k = 0 to n - 1 step by
        t += 1
    next
    return t
end function
print count_in(10, 3); " "; count_in(10, 1); " "; count_in(0, 1)
' to b: both ends counted, b and the step read once
for k = 1 to 3
    print "to"; " "; k
next
for k = 3 to 1 step -1
    print "down"; " "; k
next
for k = 1 to 0
    print "never"
next
var reads = 0
function last() as int
    reads += 1
    return 3
end function
for k = 1 to last()
    print "once"; " "; k; " "; reads
next
' do: a test after loop, the body run once first; continue goes to the test
var d = 0
do
    d += 1
loop until d >= 3
print "until"; " "; d
do
    d += 1
loop while d < 2
print "while"; " "; d
var c = 0
d = 0
do
    d += 1
    if d mod 2 = 0 then continue
    c += 1
loop while d < 6
print "continue"; " "; d; " "; c
' next is a keyword where a statement starts, a name elsewhere
function next() as int
    return 7
end function
print next()
