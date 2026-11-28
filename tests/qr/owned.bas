option dialect quickr
' records holding lists: built, copied, in lists, returned
type Bag
    name as string
    items as list[int]
end type

function make(n as int) as Bag
    var b as Bag
    b.name = "bag" + string(n)
    for i in 0..n
        b.items.push(i)
    next
    return b
end function

function sum(a as list[int]) as int
    var t = 0
    for x in a
        t += x
    next
    return t
end function

function count(n as int) as int
    var l as list[int]
    for i in 0..n
        l.push(i)
    next
    return len(l)
end function

print count(3); " "; count(5)
let b = make(4)
print b.name; " "; len(b.items); " "; sum(b.items)
var c = b
c.items.push(100)
print len(b.items); " "; len(c.items); " "; sum(c.items)
var bags as list[Bag]
bags.push(make(2))
bags.push(make(3))
print bags[1].name; " "; sum(bags[1].items); " "; len(bags[0].items)
bags[0].items.push(7)
print sum(bags[0].items)
bags.remove(0)
print len(bags); " "; bags[0].name
function rev(a as list[int]) as list[int]
    var r as list[int]
    for i in 0..len(a)
        r.push(a[len(a) - 1 - i])
    next
    return r
end function
let r = rev(rev(b.items))
print r[0]; " "; r[3]
var e as Bag
e.items = rev(b.items)
print e.items[0]
