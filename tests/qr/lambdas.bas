option dialect quickr
' lambdas inlined per call, captures by reference, function values
type Vec
    x as int
    y as int
end type
type Button
    label as string
    on as function(int) as int
end type
function apply(f as function(int) as int, x as int) as int
    return f(x)
end function
function twice(f as function(int) as int, x as int) as int
    return f(f(x))
end function
function pass_on(f as function(int) as int, x as int) as int
    return apply(f, x) + 1
end function
function each(a as int[], f as function(int))
    for x in a
        f(x)
    next
end function
function total(a as int[], f as function(int) as int) as int
    var t = 0
    for x in a
        t += f(x)
    next
    return t
end function
function map_all[T, U](a as T[], f as function(T) as U) as U[]
    var r as U[]
    for x in a
        r.push(f(x))
    next
    return r
end function
function vmap(v as Vec, f as function(Vec) as Vec) as Vec
    return f(v)
end function
function join(a as string[], f as function(string) as string) as string
    var s = ""
    for x in a
        s = s + f(x) + ","
    next
    return s
end function
function double_it(x as int) as int
    return x * 2
end function
function shout(s as string) as string
    return s + "!"
end function
function through(f as function(int) as int) as int
    return apply(fn(v) f(v) + 1, 2)
end function
function run()
    var k = 10
    print apply(fn(v) v + k, 5); " "; twice(fn(v) v * k, 3); " "; apply(double_it, 21)
    var xs as int[]
    for i in 0..5
        xs.push(i)
    next
    var sum = 0
    each(xs, fn(x as int)
        ' a block's own lines
        sum += x
        k -= 1
    end fn)
    print sum; " "; k
    let base = 100
    print total(xs, fn(v)
        let w = v * 2
        return w + base
    end fn)
    let names = map_all(xs, fn(x) f"n{x}")
    print names[0]; " "; names[4]; " "; len(names); " "; map_all(xs, fn(x) x / 2)[3]
    for y in xs
        if y > 2 then print apply(fn(v) v * y, 10)
    next
    print pass_on(fn(v) v - xs[1], 10); " "; apply(fn(a) apply(fn(b) a * b + xs[4], 2), 3)
    let w = vmap(Vec(1, 2), fn(p) Vec(p.y + xs[3], p.x))
    print w.x; " "; w.y
    var words as string[]
    words.push("a")
    words.push("bc")
    print join(words, fn(s) s + s); " "; join(words, shout)
end function
run()
let g as function(int) as int = double_it
print g(4); " "; apply(g, 6); " "; through(g)
let sh = shout
print sh("hey") + sh("x")
var b = Button("go", double_it)
print b.label; " "; b.on(5)
var zs as int[]
zs.push(7)
for z in zs
    print apply(fn(v) v + z, 1)
next
