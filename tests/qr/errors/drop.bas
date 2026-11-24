' OPTION DIALECT QUICKR
' drop: its errors
type Res
    name as string
end type
function drop(mut r as Res)
end function
function drop(mut r as Res)
end function
function drop(r as int)
end function
function drop(mut n as int)
end function
type Node
    kids as Node[]
    r as Res
end type
enum Shape
    held(r as Res)
end enum
function make() as Res
    return Res("m")
end function
function gen() yields int
    let r = Res("g")
    yield 1
end function
var a = Res("a")
var b = Res("b")
let c = copy(a)
print make().name
var m as dict[string, Res]
a, b = b, a
for x in gen()
    print x
next
