' OPTION DIALECT QUICKR
' lambdas inlined per call, captures by reference, fn values
type Vec
    x as int
    y as int
end type
type Button
    label as string
    on as fn(int) as int
end type
fn apply(f as fn(int) as int, x as int) as int
    return f(x)
end fn
fn twice(f as fn(int) as int, x as int) as int
    return f(f(x))
end fn
fn pass_on(f as fn(int) as int, x as int) as int
    return apply(f, x) + 1
end fn
fn each(a as int[], f as fn(int))
    for x in a
        f(x)
    end for
end fn
fn total(a as int[], f as fn(int) as int) as int
    var t = 0
    for x in a
        t += f(x)
    end for
    return t
end fn
fn map_all[T, U](a as T[], f as fn(T) as U) as U[]
    var r as U[]
    for x in a
        r.push(f(x))
    end for
    return r
end fn
fn vmap(v as Vec, f as fn(Vec) as Vec) as Vec
    return f(v)
end fn
fn join(a as string[], f as fn(string) as string) as string
    var s = ""
    for x in a
        s = s + f(x) + ","
    end for
    return s
end fn
fn double_it(x as int) as int
    return x * 2
end fn
fn shout(s as string) as string
    return s + "!"
end fn
fn through(f as fn(int) as int) as int
    return apply(fn(v) f(v) + 1, 2)
end fn
fn run()
    var k = 10
    print(apply(fn(v) v + k, 5), twice(fn(v) v * k, 3), apply(double_it, 21))
    var xs as int[]
    for i in 0..5
        xs.push(i)
    end for
    var sum = 0
    each(xs, fn(x as int)
        ' a block's own lines
        sum += x
        k -= 1
    end fn)
    print(sum, k)
    let base = 100
    print(total(xs, fn(v)
        let w = v * 2
        return w + base
    end fn))
    let names = map_all(xs, fn(x) f"n{x}")
    print(names[0], names[4], len(names), map_all(xs, fn(x) x / 2)[3])
    for y in xs
        if y > 2 then print(apply(fn(v) v * y, 10))
    end for
    print(pass_on(fn(v) v - xs[1], 10), apply(fn(a) apply(fn(b) a * b + xs[4], 2), 3))
    let w = vmap(Vec(1, 2), fn(p) Vec(p.y + xs[3], p.x))
    print(w.x, w.y)
    var words as string[]
    words.push("a")
    words.push("bc")
    print(join(words, fn(s) s + s), join(words, shout))
end fn
run()
let g as fn(int) as int = double_it
print(g(4), apply(g, 6), through(g))
let sh = shout
print(sh("hey") + sh("x"))
var b = Button("go", double_it)
print(b.label, b.on(5))
var zs as int[]
zs.push(7)
for z in zs
    print(apply(fn(v) v + z, 1))
end for
