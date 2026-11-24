' OPTION DIALECT QUICKR
' jumps within a line: labels, and QJ where a string is on the stack
var x = 3
let s = "m"
print("a" + ("b" if x > 1 else "c") + "d")
print(s + ("y" if x > 5 else "n"), ("p" if x < 0 else "q") + s)
print("a" < s < "z", "a" < s + "x" < "b", s = "m" and x = 3)
fn both(t as string, ok as bool) as string
    return t + string(ok)
end fn
print(both(s + "!", x > 1 and x < 5), both("no", x > 5 or x < 0))
print(((x > 1 and x < 5) or (x > 9 and x < 20)) and not (x = 4))
print(1 if x > 2 and x < 4 else 2 if x > 0 else 3)
print(0 < x < 5 < 10, 0 < x < 2 < 10, x + (10 if x > 1 else 20))
var n = 0
while n < 10 and not (n > 2 and n mod 2 = 1)
    n += 1
end while
print(n)
