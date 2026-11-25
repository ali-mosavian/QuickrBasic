' OPTION DIALECT QUICKR
import util

type Spot
    x as int
    y as single
    tag as string
end type

var made as int = util.start() + 1

function area(w as single, h as single) as single
    return w * h
end function

function scale(mut p as Spot, by as int)
    p.x = p.x * by
    p.y = p.y * single(by)
    p.tag = p.tag + "*"
end function

function bump(mut n as int, mut s as short, mut b as bool)
    n += 1
    s -= 1
    b = not b
end function

function greet(who as string) as string
    return "hi " + who
end function

function half(d as double) as double
    return d / 2
end function

function count() as int
    return made
end function

function make(x as int) as Spot
    return Spot(x, 2.5, "made")
end function

function norm(p as Spot) as single
    return single(p.x) + p.y
end function

function isbig(n as int) as bool
    return n > 10
end function

function quot(a as int, b as int) as int
    return a div b
end function
