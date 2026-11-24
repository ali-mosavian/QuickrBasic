' OPTION DIALECT QUICKR
' calls: arguments, defaults, names, results
function f(a as int, b = 2) as int
    return a + b
end function
print(f())
print(f(1, 2, 3))
print(f(b: 1))
print(f(1, c: 2))
print(f(a: 1, 2))
print(f(1, a: 2))
function g(a = 1, b as int)
end function
function h(a as int = "x")
end function
function k(a as int = f(1))
end function
function m(mut a = 1)
end function
let v = g(1, 2)
function r() as int
    print(1)
end function
function q() as int
    if true then return 1
end function
function w()
    return 1
end function
function e() as int
    return
end function
