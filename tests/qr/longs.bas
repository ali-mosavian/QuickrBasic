' OPTION DIALECT QUICKR
' long, ulong and uint on their own ops: a loop's sum, compares that branch
var t as long = 0
for i in 0..1000
    t += i * 3
next
var n as long = 5_000_000_000
var k as long = 0
while k < n
    k += 1_000_000_000
wend
var c as long = 7
c -= 10
c *= -4
print t; " "; k; " "; c; " "; k > n; " "; k <= n; " "; c <> 12
' long: div mod shr shl and or xor not -, by variables
var a as long = -7_000_000_001
let b as long = 2
print a div b; " "; a mod b; " "; a mod -3; " "; -a
print a shr 1; " "; b shl 40; " "; a shr 63; " "; a and 255; " "; b or 1; " "; a xor b; " "; not b
print a < b; " "; a >= b; " "; a = -7_000_000_001
' conversions
let d as double = 2.5e10
let i as int = -5
let ui as uint = 4_000_000_000
print long(d); " "; long(-d); " "; int(b * 1000); " "; double(a); " "; a + i; " "; a - ui
' uint and ulong
var u as uint = 4_000_000_000
let w as uint = 3
print u + w; " "; u - w; " "; u div w; " "; u mod w; " "; u > w; " "; u shr 4
var ul as ulong = 18_000_000_000_000_000_000
let x as ulong = 7
print ul + x; " "; ul div x; " "; ul mod x; " "; ul > x; " "; ul shr 60; " "; ul + ui
' wrap keeps each type's bits
print wrap(ul * x); " "; wrap(a * a); " "; wrap(u * u); " "; wrap(-u); " "; wrap(i * 1_000_000_000)
print a + c; " "; a - 5; " "; a + 200; " "; c * a; " "; ulong(c); " "; uint(k div 2); " "; wrap(a + a); " "; wrap(x - ul); " "; w * 1_000_000_000
if b < a * -2 then print "popped"
' a function's locals: in its frame
function tri(n as long) as long
    var s as long = 0
    var j as long = 0
    let m = n
    while j < m
        j += 1
        s = j + s
        s = s * 1
    wend
    return s - n + n
end function
print tri(100_000)
