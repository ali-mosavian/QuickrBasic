option dialect quickr
' drop: its errors
type Res
    name as string
    fn drop(mut self)
    end fn
    fn drop(mut self)
    end fn
end type
type Plain
    n as int
    fn drop(self)
    end fn
end type
function drop(mut r as Res)
end function
enum Mode
    on
    fn drop(mut self)
    end fn
end enum
type Node
    kids as list[Node]
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
let e = a
type Holder
    r as Res
end type
var h = Holder(Res("h"))
let hr = h.r
print make().name
a.drop()
let d = Res.drop
var m as dict[string, Res]
a, b = b, a
for x in gen()
    print x
next
