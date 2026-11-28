option dialect quickr
' lists of lists, lists as dict values: each its own, copied whole
type Board
    name as string
    cells as list[list[string]]
end type
function make(n as int) as list[list[string]]
    var out as list[list[string]]
    for i in 0..n
        var r as list[string]
        for j in 0..n
            r.push(f"{i}{j}")
        next
        out.push(r)
    next
    return out
end function
function total(g as list[list[string]]) as int
    var n = 0
    for r in g
        for s in r
            n += len(s)
        next
    next
    return n
end function
function grow(mut g as list[list[string]])
    for mut r in g
        r.push("x")
    next
    var extra as list[string]
    extra.push("y")
    g.push(extra)
end function
var grid as list[list[int]]
for i in 0..4
    var row as list[int]
    for j in 0..i
        row.push(i * 10 + j)
    next
    grid.push(row)
next
grid[2][1] = 99
grid[1].push(7)
print len(grid); " "; len(grid[3]); " "; grid[3][2]; " "; grid[1][1]
var copy = grid
copy[0].push(5)
print len(grid[0]); " "; len(copy[0])
let last = grid.pop()
print len(last); " "; last[2]; " "; len(grid)
grid.remove(0)
grid.insert(0, last)
grid.swap(0, 1)
grid.sort(fn(a, b) len(a) < len(b))
for r in grid
    for x in r
        print x; " ";
    next
    print "/ ";
next
print
print grid.map(fn(r) len(r)).sum(); " "; grid[1..].count()
var b as Board
b.cells = make(3)
grow(b.cells)
print total(b.cells); " "; len(b.cells); " "; b.cells[3][0]
var c = b
c.cells[0][0] = "zz"
print b.cells[0][0]; " "; c.cells[0][0]
for i in 0..500
    var junk = make(4)
    b.cells[0][1] = f"{i}{len(junk)}"
next
print b.cells[0][1]; " "; total(b.cells)
var deep as list[list[list[int]]]
for i in 0..3
    var mid as list[list[int]]
    for j in 0..3
        var inner as list[int]
        inner.push(i * 100 + j)
        mid.push(inner)
    next
    deep.push(mid)
next
print deep[2][1][0]; " "; len(deep[0])
var d as dict[string, list[int]]
d["a"] = grid[0]
d["a"].push(3)
var none_yet as list[int]
d["b"] = none_yet
d["b"].push(4)
print len(d["a"]); " "; len(d["b"]); " "; len(grid[0])
let got = d.get("a")
if got is not none then print len(got)
var d2 = d
d2["b"].push(9)
d.remove("a")
for k, v in d
    print k; " "; len(v); " "; v[0]
next
print len(d2["b"]); " "; len(d2)
