' OPTION DIALECT QUICKR
' calls: arguments, defaults, names, results
fn f(a as int, b = 2) as int
    return a + b
end fn
print(f())
print(f(1, 2, 3))
print(f(b: 1))
print(f(1, c: 2))
print(f(a: 1, 2))
print(f(1, a: 2))
fn g(a = 1, b as int)
end fn
fn h(a as int = "x")
end fn
fn k(a as int = f(1))
end fn
fn m(mut a = 1)
end fn
let v = g(1, 2)
fn r() as int
    print(1)
end fn
fn q() as int
    if true then return 1
end fn
fn w()
    return 1
end fn
fn e() as int
    return
end fn
