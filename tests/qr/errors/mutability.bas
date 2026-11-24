' OPTION DIALECT QUICKR
' what can't change: a let, a const, a parameter, a loop's counter
let a = 1
a = 2
const C = 1
C += 1
fn f(n as int)
    n = 1
end fn
for i in 0..3
    i = 5
end for
fn g(mut n as int)
end fn
var v = 1
g(v)
g(mut a)
g(mut 1 + 2)
f(mut v)
fn h(mut x as int, y as int)
end fn
h(mut v, v)
var d as double = 1
g(mut d)
