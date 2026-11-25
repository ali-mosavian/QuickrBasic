' OPTION DIALECT QUICKR
' for to an int's last value ends there: next once stepped past
' it first, Illegal function call
let top = 2147483647
let low = -2147483648
for i = 2147483646 to 2147483647
    print "up"; " "; i
next
for i = 2147483640 to 2147483647 step 2
    print "up2"; " "; i
next
for i = -2147483646 to -2147483648 step -1
    print "down"; " "; i
next
for i = -2147483640 to -2147483648 step -3
    print "down3"; " "; i
next
for i = 0 to top step top
    print "far"; " "; i
next
let most = low
for i = 0 to low step most
    print "farthest"; " "; i
next
for i in (low..low + 2).rev()
    print "rev"; " "; i
next
print "chain"; " "; len((low..low + 3).rev().collect())
let up = 1
let dn = -1
for i = 2147483646 to top step up
    print "run up"; " "; i
next
for i = -2147483647 to low step dn
    print "run down"; " "; i
next
function local_count(first as int, last as int, by as int) as int
    var n = 0
    for i = first to last step by
        n += 1
    next
    for i = first to last
        n += 1
    next
    return n
end function
print "local"; " "; local_count(2147483645, top, 1)
print "local"; " "; local_count(2147483645, top, 2)
function counter(first as int, last as int, by as int) yields int
    for i = first to last step by
        yield i
    next
end function
function counter_up(first as int) yields int
    for i = first to 2147483647
        yield i
    next
end function
for x in counter(-2147483646, low, -1)
    print "gen"; " "; x
next
for x in counter_up(2147483646)
    print "gen up"; " "; x
next
var kept = counter(2147483645, top, 2)
print "kept"; " "; kept.next() or else 0; " "; kept.next() or else 0; " "; kept.next() or else 0
var kept_up = counter_up(2147483646)
print "kept up"; " "; kept_up.next() or else 0; " "; kept_up.next() or else 0; " "; kept_up.next() or else 0
print "done"
