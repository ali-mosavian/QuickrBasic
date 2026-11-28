option dialect quickr
' a call can't borrow a module record or list the callee, or a function it
' reaches, writes; a number passes by value
var count = 0
type Point
    x as int
end type
var pt = Point(x: 1)
var items as list[int]
function bump(n as int)
    count += 1
end function
function outer(n as int)
    bump(n)
end function
function grow(mut p as Point)
    pt.x += 1
end function
function add(xs as list[int])
    items.push(1)
end function
grow(pt)
add(items)
function apply(n as int, g as function(int))
    g(n)
end function
var drops = 0
type Res
    n as int
    fn drop(mut self)
        drops += 1
    end fn
end type
function hold(n as int)
    let r = Res(1)
end function
function read(n as int) as int
    return n + count
end function
var other = 0
function other_bump(n as int)
    other += n
end function
print read(count); " "; count + 1
other_bump(count)
bump(count + 1)
