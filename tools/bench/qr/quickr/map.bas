' OPTION DIALECT QUICKR
var m as int[int]
for i in 0..4000
    m[i * 7919] = i
next
var sum = 0
for r in 0..5
    for i in 0..4000
        sum += m[i * 7919]
    next
next
print(len(m), sum)
