option dialect quickr
function fib(n as int) as int
    if n < 2 then return n
    return fib(n - 1) + fib(n - 2)
end function
var s = 0
for i in 0..30000000
    s += i mod 7
next
print fib(32); " "; s
