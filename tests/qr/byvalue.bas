option dialect quickr
' numbers, bools, plain enums and strings pass by value: a call changing
' the variable passed leaves its parameter as it was
var count = 0
var name = "ada"
var on = true
enum Mode
    walk
    run
end enum
var mode = Mode.walk
function bump(n as int) as int
    count += 1
    return n
end function
print bump(count); " "; count
function rename(s as string) as string
    name = "bo"
    return s
end function
print rename(name); " "; name
function flip(b as bool) as bool
    on = not on
    return b
end function
print flip(on); " "; on
function switch(m as Mode) as Mode
    mode = Mode.run
    return m
end function
print switch(mode); " "; mode
function twice(mut x as int, y as int)
    x *= 2
    print x; " "; y
end function
var v = 3
twice(v, v)
type P
    x as int
end type
function moved(mut p as P, x as int)
    p.x = 9
    print p.x; " "; x
end function
var p = P(4)
moved(p, p.x)
function apply(n as int, g as function(int))
    g(1)
    print n
end function
var total = 5
apply(total, fn(x as int)
    total += x
end fn)
print total
function local_case()
    var t = 1
    apply(t, fn(x as int)
        t += x
    end fn)
    print t
end function
local_case()
