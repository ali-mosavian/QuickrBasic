' OPTION DIALECT QUICKR
' print: each value spelled; ; joins, , moves to the next 14-column zone, a newline after
print 1; " "; 2.5; " "; "x"; " "; true
print
print "alone"
print -7; " "; 0; " "; 2147483647; " "; -2147483648
print false; " "; true
print 0.1; " "; 1.0; " "; -0.5; " "; 100.0; " "; 1e15; " "; 1e16; " "; 1e-4; " "; 1e-5
print 1.0 / 3; " "; 2.0 / 3; " "; 123456789.125
let s as single = 0.1
let t as single = 1.0 / 3
print s; " "; t; " "; single(1e7); " "; single(1e8); " "; single(16777216)
print 1.5e300; " "; -2.5e-300
print ""; " "; "a"; " "; ""
print 1; 2; "x"
print "a", "b", 3
print "held";
print " on"
print 1,
print 2
print
print (1 + 2) * 2
print "abcdefghijklmnop", "q"
if true then print "then"; else print "else"
print "after"
