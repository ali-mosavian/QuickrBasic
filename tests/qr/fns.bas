' OPTION DIALECT QUICKR
' functions: called as a value or a statement, recursion, mut, defaults, names
function fact(n as int) as int
    if n <= 1 then return 1
    return n * fact(n - 1)
end function
print(fact(10))
function fib(n as int) as int
    if n < 2 then return n
    return fib(n - 1) + fib(n - 2)
end function
print(fib(20))
function hello(who as string)
    print("hello", who)
end function
hello("you")
function early(n as int)
    if n < 0 then return
    print("non-negative", n)
end function
early(-1)
early(2)
function swap(mut a as int, mut b as int)
    let t = a
    a = b
    b = t
end function
var x = 1
var y = 2
swap(mut x, mut y)
print(x, y)
function add_to(mut total as double, n as int, times = 1)
    total += n * times
end function
var acc as double = 0.5
add_to(mut acc, 2)
add_to(mut acc, 3, times: 10)
add_to(mut acc, times: 2, n: 1)
print(acc)
function label(text as string, left = "[", right = "]") as string
    return left + text + right
end function
print(label("a"), label("b", "<"), label("c", right: ")"))
function pass_on(mut n as int)
    swap(mut n, mut y)
end function
var z = 9
pass_on(mut z)
print(z, y)
function half(v as double) as double
    return v / 2
end function
print(half(3), half(x))
function is_even(n as int) as bool
    return n mod 2 = 0
end function
function is_odd(n as int) as bool
    if n = 0 then return false
    return is_even(n - 1)
end function
print(is_odd(7), is_even(7))
function reads(n as int) as int
    return n + 1
end function
var r = 5
print(reads(r), r)
