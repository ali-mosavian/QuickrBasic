' OPTION DIALECT QUICKR
' operators
var a = 7
var b = 2
print(a + b, a - b, a * b, a / b, a div b, a mod b, a ** b)
print(-a div b, -a mod b, a div -b, 2 ** 10, 2.0 ** 0.5)
print(1 + 2 * 3, (1 + 2) * 3, 2 ** 3 ** 2, -2 ** 2)
print(a = 7, a <> 7, a < b, a <= 7, a > b, a >= 8)
print(1 < 2 < 3, 1 < 3 < 2, 1 <= 1 < 2 <= 2)
print("a" < "b", "ab" = "ab", "b" > "abc", "x" <> "y")
print(a in 0..8, a in 0..7, a not in 0..7, -1 in 0..7)
print(true and true, true and false, false or true, false or false)
print(not true, true xor false, true xor true, not (a > b))
print(6 & 3, 6 | 3, 6 ^ 3, ~5, 1 << 31, -8 >> 1, 255 >> 4)
print("con" + "cat", "x" if a > b else "y", 1 if a < b else 2)
var calls = 0
function tick(v as bool) as bool
    calls += 1
    return v
end function
print(false and tick(true), true or tick(true), calls)
print(true and tick(true), false or tick(false), calls)
function mid() as int
    calls += 1
    return 5
end function
print(0 < mid() < 10, calls)
print(mid() in 0..3, calls)
print(tick(true) if a > b else tick(false), calls)
let v as single = 2.5
print(v * 2, v / 2, v + 0.25, v ** 2)
