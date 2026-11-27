option dialect quickr
' kept generators as values: copied, passed mut, returned, used by a
' lambda, looped; return x the last yield; an inline one's mut
function upto(n as int) yields int
    for i in 0..n
        yield i
    next
    return 100 + n
end function
function words(s as string) yields string
    var w = ""
    for c in s
        if c = 32 then
            yield w
            w = ""
        else
            w = w + text_of(c)
        end if
    next
    yield w
end function
function text_of(c as int) as string
    return "x" if c = 120 else "y"
end function
function drain(mut it as upto) as int
    var t = 0
    for x in it
        t += x
    next
    return t
end function
function make(n as int) as upto
    return upto(n)
end function
function bump(mut a as int) yields int
    a += 1
    yield a
    a += 10
    yield a
end function
function apply(f as function(int) as int) as int
    return f(1)
end function
var a = upto(3)
print a.next() or else -1
var b = a
print a.next() or else -1; " "; b.next() or else -1; " "; b.next() or else -1
print drain(a)
var m = make(2)
print m.next() or else -1; " "; m.next() or else -1; " "; m.next() or else -1; " "; m.next() or else -1
var w = words("xx yx")
var w2 = w
print w.next() or else "-"; " "; w2.next() or else "-"; " "; w.next() or else "-"; " "; w.next() or else "-"
var c = upto(5)
print apply(fn(k) k + (c.next() or else 0)); " "; apply(fn(k) k + (c.next() or else 0))
var v = 5
for x in bump(v)
    print x; " "; v
next
print v
