' OPTION DIALECT QUICKR
' literals, comments and lines continued inside brackets
print 0xff; " "; 0XFF; " "; 0x7fffffff; " "; 0b1010; " "; 0b0
print 1_000; " "; 1_000_000; " "; 0xff_ff; " "; 0b1111_0000
print 2.5; " "; 0.5; " "; 10.25; " "; 1_000.5; " "; 2e3; " "; 1.5e-3; " "; 3E2
print "tab\tend"; " "; "q\"q"; " "; "back\\slash"
print "two\nlines"
print true; " "; false  ' a comment after a statement
' a whole-line comment
print 1; " "; 2; " "; 3
let sum = (1 +
           2)
print sum
print "it's"
