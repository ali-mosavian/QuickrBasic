defint a-z
declare function fib (n)
s = 0
for i = 0 to 29999999
    s = s + i mod 7
next
print fib(32); s
function fib (n)
    if n < 2 then fib = n: exit function
    fib = fib(n - 1) + fib(n - 2)
end function
