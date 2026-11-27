option dialect quickr
var x as single = 0
for k in 0..300
    for i in 0..1000
        x = single(i) * 0.5 - single(i)
    next
next
print x
