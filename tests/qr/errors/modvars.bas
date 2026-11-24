' OPTION DIALECT QUICKR
' a call can't borrow a module var the callee, or a function it reaches, writes
var count = 0
type Point
    x as int
end type
var pt = Point(x: 1)
var items as int[]
function bump(n as int)
    count += 1
end function
function outer(n as int)
    bump(n)
end function
function grow(mut p as Point)
    pt.x += 1
end function
function add(xs as int[])
    items.push(1)
end function
bump(count)
outer(count)
grow(pt)
add(items)
let f = bump
f(count)
function apply(n as int, g as function(int))
    g(n)
end function
apply(count, fn(x as int)
    count += x
end fn)
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
hold(drops)
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
