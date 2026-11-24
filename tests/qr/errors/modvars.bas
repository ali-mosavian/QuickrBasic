' OPTION DIALECT QUICKR
' a call can't borrow a module var the callee, or a fn it reaches, writes
var count = 0
type Point
    x as int
end type
var pt = Point(x: 1)
var items as int[]
fn bump(n as int)
    count += 1
end fn
fn outer(n as int)
    bump(n)
end fn
fn grow(mut p as Point)
    pt.x += 1
end fn
fn add(xs as int[])
    items.push(1)
end fn
bump(count)
outer(count)
grow(mut pt)
add(items)
let f = bump
f(count)
fn apply(n as int, g as fn(int))
    g(n)
end fn
apply(count, fn(x as int)
    count += x
end fn)
var drops = 0
type Res
    n as int
end type
fn drop(mut r as Res)
    drops += 1
end fn
fn hold(n as int)
    let r = Res(1)
end fn
hold(drops)
fn read(n as int) as int
    return n + count
end fn
var other = 0
fn other_bump(n as int)
    other += n
end fn
print(read(count), count + 1)
other_bump(count)
bump(count + 1)
