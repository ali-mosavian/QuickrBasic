' OPTION DIALECT QUICKR
function total(a as int[]) as int
    var s = 0
    for k in 0..4000
        for x in a
            s += x
        next
    next
    return s
end function

var a as int[]
for i in 0..1000
    a.push(i mod 500)
next
print(total(a))
