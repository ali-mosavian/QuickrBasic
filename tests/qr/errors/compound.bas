option dialect quickr
' records, enums, lists: their errors
type Vec
    x as single
    y as single
end type
type Loop
    me as Loop
end type
enum Mode
    walk
    jump
end enum
let v = Vec(1, 2, 3)
let w = Vec(z: 1)
print v.z
v.x = 2
print v
var c as int[]
for x in c
    c.push(x)
    x = 2
next
for mut x in c
    c.remove(0)
    print len(c); " "; c[0]
next
select case Mode.walk
    case Mode.walk
        print 1
end select
function f(m as Mode) as int
    select case m
        case Mode.walk
            return 1
        case Mode.jump
            return 2
    end select
end function
var n as int[4]
n.push(1)
print n[4]
function g(mut v as Vec)
end function
var u = Vec(1, 2)
g(u.x)
let k = f"{v}"
let j = f"{3:q}"
