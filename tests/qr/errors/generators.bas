' OPTION DIALECT QUICKR
' generators: yield's type, loops over itself, used as a value, let, next(), a kept one's mut
function upto(n as int) yields int
    var i = 0
    while i < n
        yield i
        i += 1
    end while
end function
function bad(n as int) yields int
    yield "x"
    return 5
end function
function loops(n as int) yields int
    for x in loops(n)
        yield x
    end for
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
print(b)
var m = b.next() ?? 0
var c = changes(mut m)
print(b.size())
let s = upto(4)
for x in upto(2)
    x = 5
end for
for i, x in upto(2)
    print(x)
end for
for x in bad(1)
    print(x)
end for
for x in loops(1)
    print(x)
end for
let f = upto
for x in changes(mut m)
    print(x)
end for
print(b.next(1) ?? 0)
upto(3)
