option dialect quickr
function steps(n as int) yields int
    var x = n
    while x <> 1
        if x mod 2 = 0 then
            x = x div 2
        else
            x = 3 * x + 1
        end if
        yield x
    wend
end function
var t = 0
for s in 1..3000
    for v in steps(s)
        t += v mod 7
    next
next
print t
