option dialect quickr
' generators: yield's type, loops over itself, used as a value, let, next(), a kept one's mut
function upto(n as int) yields int
    var i = 0
    while i < n
        yield i
        i += 1
    wend
end function
function bad(n as int) yields int
    yield "x"
    return 5
end function
function loops(n as int) yields int
    for x in loops(n)
        yield x
    next
end function
function both(n as int) as int yields int
    return 1
end function
function plain()
    yield 1
end function
function changes(mut n as int) yields int
    yield n
end function
let a = upto(3)
var b = upto(3)
print b
var m = b.next() or else 0
var c = changes(m)
print b.size()
let s = upto(4)
for x in upto(2)
    x = 5
next
for i, x in upto(2)
    print x
next
for x in bad(1)
    print x
next
for x in loops(1)
    print x
next
let f = upto
for x in changes(m)
    print x
next
print b.next(1) or else 0
upto(3)
