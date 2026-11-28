option dialect quickr
' A fn's record local holding an array or a list kept it
' past the return: 600 calls ran out of memory.
type Run
    xs as int[]
end type
type Bag
    items as list[int]
end type
function copied(r as Run) as int
    let c = r
    return c.xs[0]
end function
function made() as int
    var c as Run
    c.xs = int[200000]
    return len(c.xs)
end function
function pushed() as int
    var b as Bag
    for i in 0..30000
        b.items.push(i)
    next
    return len(b.items)
end function
var r as Run
r.xs = int[200000]
r.xs[0] = 7
var s = 0
for k in 0..600
    s = copied(r) + made() + pushed()
next
print s
