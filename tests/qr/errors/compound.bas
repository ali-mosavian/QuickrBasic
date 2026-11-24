' OPTION DIALECT QUICKR
' records, enums, lists: their errors
type Vec
    x as single
    y as single
end type
type Bag
    items as int[]
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
var a as int[]
var b = a
a.push(1)
var c as int[]
for x in c
    c.push(x)
    x = 2
next
for mut x in c
    c.remove(0)
    print len(c); " "; c[0]
next
var bag as Bag
var bag2 = bag
print bag.items[0]
var bag3 = bag.items
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
g(mut u.x)
let k = f"{v}"
let j = f"{3:q}"
var ss as int[]
for i in 0..3
    var t = ss
next
var l1 as int[]
var l2 as int[]
var l3 = l1 if len(l2) > 0 else l2
print len(l1)
