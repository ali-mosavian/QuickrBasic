option dialect quickr
' 1,000 live lists, a list of lists. The arrays' headers had a
' fixed room of about 360: Out of memory partway.
var big as list[list[int]]
for r = 0 to 999
    var row as list[int]
    row.push(r)
    row.push(2 * r)
    big.push(row)
next
var sum = 0
for r = 0 to 999
    sum = sum + big[r][1]
next
print len(big); " "; big[999][0]; " "; sum
