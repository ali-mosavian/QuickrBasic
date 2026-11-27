option dialect quickr
type Acc
    n as int
    s as int
end type

function walk(mut a as Acc)
    for i in 0..10000000
        a.n += 3
        a.s = (a.s + a.n) mod 997
    next
end function

var a = Acc(0, 0)
walk(a)
print a.n; " "; a.s
