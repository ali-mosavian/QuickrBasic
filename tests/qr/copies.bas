' OPTION DIALECT QUICKR
' assignment copies; a source used nowhere after moves
type Bag
    items as int[]
end type
var a as int[]
a.push(1)
var b = a
b.push(2)
print len(a); " "; len(b)
var bag as Bag
bag.items.push(5)
var bag2 = bag
bag2.items.push(6)
print len(bag.items); " "; len(bag2.items)
var its = bag.items
its.push(7)
print len(bag.items); " "; len(its)
function grown(xs as int[]) as int[]
    var ys = xs
    ys.push(0)
    return ys
end function
print len(grown(a)); " "; len(a)
var ss as int[]
var total = 0
for i in 0..3
    var t = ss
    t.push(i)
    total += len(t)
next
print total; " "; len(ss)
var l1 as int[]
l1.push(1)
var l2 as int[]
var l3 = l1 if len(l2) > 0 else l2
print len(l1); " "; len(l3)
var m as dict[string, int]
m["a"] = 1
let n = m
m["b"] = 2
print len(n); " "; len(m)
var seen as int[]
function count_seen() as int
    return len(seen)
end function
var mine = seen
mine.push(1)
print count_seen(); " "; len(mine)
var last = b
print len(last)
