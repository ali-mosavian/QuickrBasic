' OPTION DIALECT QUICKR
' chains: map, filter, take, skip, zip, rev; sum .. collect; one loop each, a for's too
function upto(n as int) yields int
    var i = 0
    while i < n
        yield i
        i += 1
    wend
end function
function chained(n as int) yields int
    for x in (0..n).filter(fn(x) x mod 2 = 1)
        yield x * 100
    next
end function
function twice(x as int) as int
    return x * 2
end function
function total(a as int[], f as function(int) as int) as int
    return a.map(f).sum()
end function
function evens(a as int[]) yields int
    for x in a.filter(fn(x) x mod 2 = 0).rev()
        yield x
    next
end function
function sums(n as int) yields int
    for i in 1..n
        yield (0..i).sum()
    next
end function
var a as int[]
for i in 0..10
    a.push(i)
next
print a.sum(); " "; a.count(); " "; a.filter(fn(x) x mod 2 = 0).sum()
print a.map(fn(x) x * x).filter(fn(v) v > 10).sum()
print (0..5).sum(); " "; (0..5).rev().collect()[0]; " "; len(a.collect())
print a.any(fn(x) x > 8); " "; a.all(fn(x) x < 5); " "; a.find(fn(x) x > 3) ?? -1
print a.min() ?? -1; " "; a.max() ?? -1; " "; a.filter(fn(x) x > 100).max() ?? -1
print a.take(3).sum(); " "; a.skip(7).sum(); " "; a.skip(2).take(2).sum()
print a.fold(100, fn(acc, x) acc - x)
let s = "hello"
print s.count(); " "; s.filter(fn(b) b = 108).count(); " "; s.max() ?? 0
for i in (0..4).rev()
    print i
next
for x in a.filter(fn(x) x mod 3 = 0).map(fn(x) x * 10)
    if x = 30 then continue
    print x
next
for p, q in a.zip(a).filter(fn(u, v) u > 6)
    print p + q
next
print a.zip(0..3).count(); " "; a.zip(s).map(fn(x, b) x + b).sum()
let sq = a.map(fn(x) x * x).collect()
print len(sq); " "; sq[9]
var names as string[]
names.push("bo")
names.push("ann")
print names.min() ?? ""; " "; names.max() ?? ""; " "; names.map(fn(n) n + "!").collect()[1]
var g = upto(5)
print g.map(fn(x) x + 1).sum()
print g.next() ?? -1
print upto(4).map(twice).collect()[3]; " "; upto(6).filter(fn(x) x > 2).count()
for x in upto(10).filter(fn(x) x mod 4 = 0)
    print x
next
var ch = chained(6)
print ch.next() ?? -1; " "; ch.next() ?? -1
for x in chained(4)
    print x
next
var m as int[string]
m["a"] = 1
m["bb"] = 2
print m.map(fn(k) len(k)).sum(); " "; m.filter(fn(k) m[k] > 1).collect()[0]
var arr as int[4]
arr[2] = 7
print arr.sum(); " "; arr.rev().collect()[1]
for b in "hi"
    print b
next
let k = 3
var xs as int[]
for i in 0..5
    xs.push(i)
next
print f"{xs.filter(fn(x) x > k).sum()} and {xs.map(fn(x) x * k).max() ?? 0}"
print "n=" + string(xs.sum()); " "; xs.map(fn(x) xs.filter(fn(y) y < x).count()).sum()
print total(xs, fn(x) x + k); " "; total(xs, twice)
var words as string[]
words.push("x")
words.push("yy")
print "w: " + (words.map(fn(w) w + w).max() ?? "") + "!"
var a3 as int[]
for i in 0..7
    a3.push(i)
next
var g3 = evens(copy(a3))
print g3.next() ?? -1; " "; g3.next() ?? -1; " "; g3.next() ?? -1; " "; g3.next() ?? -1; " "; g3.next() ?? -1
for x in evens(a3)
    print x
next
var sm = sums(5)
print sm.collect()[3]; " "; sm.next() ?? -1
print sums(4).fold("", fn(t, v) t + string(v) + ",")
