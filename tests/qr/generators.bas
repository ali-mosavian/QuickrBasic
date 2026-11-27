option dialect quickr
' generators: for over one inlined, a var's resumed by next(), each owning its arguments
type P
    x as int
    y as int
end type
function upto(n as int, start as int = 0) yields int
    var i = start
    while i < n
        yield i
        i += 1
    wend
end function
function once() yields int
    yield 7
end function
function three() yields string
    yield "one"
    yield "two"
    yield "three"
end function
function evens[T](a as T[]) yields T
    for i, x in a
        if i mod 2 = 0 then yield x
    next
end function
function grid(n as int) yields P
    for i in upto(n)
        for j in 0..n
            yield P(i, j)
        next
    next
end function
function until_neg(a as int[]) yields int
    for x in a
        if x < 0 then return
        yield x * 10
    next
end function
function squares(n as int) yields int
    for x in upto(n)
        yield x * x
    next
end function
function mapped(n as int, f as function(int) as int) yields int
    for x in upto(n)
        yield f(x)
    next
end function
function words(s as string) yields string
    var w = ""
    for i in 0..len(s)
        let ch = s[i..i + 1]
        if ch = " " then
            if len(w) > 0 then yield w
            w = ""
        else
            w = w + ch
        end if
    next
    if len(w) > 0 then yield w
end function
function zipped(n as int) yields int
    var a = upto(n, 1)
    var b = upto(n, 10)
    while true
        let x = a.next()
        if x is none then return
        yield x
        yield b.next() or else -1
    wend
end function
function deep(n as int) as int
    if n = 0 then return 0
    var g = upto(n)
    let first = g.next() or else 0
    let r = deep(n - 1)
    return first + (g.next() or else 0) * 10 + r * 100
end function
function big(m as dict[string, int]) yields string
    for k in m
        if m[k] > 1 then yield k
    next
end function
function sum_to(n as int) as int
    var t = 0
    for x in upto(n)
        t += x
    next
    return t
end function
function run(k as int)
    var g = grid(2)
    while true
        let p = g.next()
        if p is none then break
        print p.x; " "; p.y
    wend
    var sq = mapped(4, fn(x) x * x + k)
    print sq.next() or else -1; " "; sq.next() or else -1
    for v in mapped(3, fn(x) x + k)
        print v
    next
    var names as string[]
    names.push("ann")
    names.push("bo")
    var nm = evens(names)
    names.push("cy")
    print nm.next() or else "?"; " "; nm.next() or else "?"; " "; len(names)
end function
for x in once()
    print x + 1
next
for s in three()
    print s
next
var nums as int[]
nums.push(5)
nums.push(6)
nums.push(-7)
nums.push(8)
for v in evens(nums)
    print v
next
for x in until_neg(nums)
    print x
next
for x in squares(4)
    if x = 4 then continue
    if x = 9 then break
    print x
next
print sum_to(10)
var n = 3
for x in upto(n)
    n += 1
    print x; " "; n
next
for w in words("the quick  brown fox ")
    print w
next
var ws = words("ab cd")
print ws.next() or else "-"; " "; ws.next() or else "-"; " "; ws.next() or else "-"
for v in zipped(3)
    print v
next
print deep(3)
var m as dict[string, int]
m["a"] = 1
m["b"] = 2
m["c"] = 3
for k in big(m)
    print k
next
for x in upto(3, 1)
    for y in upto(x)
        print x; " "; y
    next
next
run(100)
for p in grid(2)
    print p.x + p.y
next
