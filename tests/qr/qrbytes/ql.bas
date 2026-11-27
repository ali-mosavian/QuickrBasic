option dialect quickr
type L
    a as ubyte[4]
    g as ubyte[]
    one as ubyte
    sea as int
end type

function sum(p as ubyte[]) as int
    var s = 0
    for x in p
        s += int(x)
    next
    return s
end function

function bump(mut p as ubyte[])
    p.push(255)
end function
