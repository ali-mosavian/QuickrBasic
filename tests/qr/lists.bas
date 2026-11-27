option dialect quickr
' lists and fixed arrays: methods, for over them, x in a, fresh lists
type Ball
    r as int
    name as string
end type

type Bag
    n as int
    items as int[]
    corners as single[4]
end type

function squares(n as int) as int[]
    var r as int[]
    for i in 0..n
        r.push(i * i)
    next
    return r
end function

function total(a as int[]) as int
    var t = 0
    for x in a
        t += x
    next
    return t
end function

function bump(mut a as int[])
    for mut x in a
        x += 1
    next
end function

let s = squares(5)
print len(s); " "; s[0]; " "; s[4]; " "; total(s)
var a = squares(3)
a.push(10)
bump(a)
for i, x in a
    print i; " "; x
next
print a.pop(); " "; len(a)
a.insert(0, 99)
a.remove(2)
for x in a
    print x
next
print 5 in a; " "; 99 in a; " "; 7 not in a
var balls as Ball[]
balls.push(Ball(1, "a"))
balls.push(Ball(2, "b"))
balls[1].r = 20
for b in balls
    print b.r; " "; b.name
next
let last = balls.pop()
print last.name; " "; len(balls)
var f as int[3]
f[1] = 7
print len(f); " "; f[0]; " "; f[1]
var bag as Bag
bag.items.push(4)
bag.items.push(5)
bag.corners[2] = 1.5
print len(bag.items); " "; bag.items[1]; " "; bag.corners[2]
var names as string[]
names.push("x")
names.push("y")
names.insert(1, "m")
for n in names
    print n
next
let c = a
print len(c)
function three() as string[]
    var r as string[]
    r.push("a")
    r.push("bb")
    r.push("c")
    return r
end function
for s in three()
    print s
next
var l = three()
print "bb" in l; " "; "z" in l; " "; l.pop(); " "; len(l)
let t = three()[1]
print t
