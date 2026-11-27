option dialect quickr
' lambdas and function values: their errors
function apply(f as function(int) as int, x as int) as int
    return f(x)
end function
function each(a as int[], f as function(int))
    for x in a
        f(x)
    next
end function
function twoargs(a as int, b as int) as int
    return a + b
end function
function bump(mut n as int) as int
    n += 1
    return n
end function
function keep(f as function(int) as int) as int
    let g = f
    return apply(fn(v) f(v), 2)
end function
function run()
    let k = 1
    var xs as int[]
    print apply(fn(a, b) a, 1)
    print apply(fn(v) v + "s", 1)
    print apply(twoargs, 1)
    print apply(bump, 1)
    let f = fn(v) v
    each(xs, fn(x as int)
        k += x
    end fn)
    each(xs, fn(x as int)
        xs.push(x)
    end fn)
    print apply(fn(v as string) 1, 1)
    print apply(fn(v) v, "no")
    print keep(fn(v) v)
    for x in xs
        each(xs, fn(y as int)
            xs.push(y)
        end fn)
    next
end function
run()
