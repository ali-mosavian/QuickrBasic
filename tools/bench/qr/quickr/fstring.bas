' OPTION DIALECT QUICKR
let s = "abc"
var n = 0
for i in 0..200000
    let t = f"<{i}:{s}:{i mod 7}:{s}>"
    n += len(t)
next
print n
