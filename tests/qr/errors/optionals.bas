' OPTION DIALECT QUICKR
' optionals and errors: their errors
function f(x as int?) as int
    if x is none then return 0
    return x * 3
end function
print(f(4), f(none))
let p = none
var q as int = none
function g() as int
    let r = parse("")?
    return r
end function
function parse(s as string) as int!
    return 1
end function
let e = Error(1, "x")
var w as int? = 3
print(w + 1)
if w is Error then print(1)
print(3 ?? 4)
let u = 5?
var v as int?? = 1
function h() as string!
    return Error("x", 2)
end function
var m as int[string]
print(m.get(1))
