option dialect quickr
var b as list[int]
for i in 0..4000
    b.push(i)
next
print len(b); " "; b[3999]
