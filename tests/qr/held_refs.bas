' OPTION DIALECT QUICKR
' Addresses into the heap on the stack while a call later in
' the statement sweeps: what they point at moved. Stale values:
' a mut element, a mut field, a field stored to, a held list's
' element, a list in a record in cells, through an inlined
' method and a fn value.
type P
    x as int
    y as int

    fn bumped(self) as int
        return churn(self.x)
    end fn
end type

type Box
    items as int[]
end type

var spacers as int[][]
for i = 1 to 8
    var s as int[]
    for k = 1 to 50
        s.push(k)
    next
    spacers.push(s)
next
var xs as int[]
xs.push(10)
xs.push(20)
var ps as P[]
ps.push(P(1, 2))
ps.push(P(3, 4))
var bx as Box
bx.items.push(5)
bx.items.push(6)
var boxes as Box[]
boxes.push(bx)
var turn = 0

' A spacer before the lists emptied, the heap swept: they move.
function churn(n as int) as int
    var gone as int[]
    spacers[turn] = gone
    turn += 1
    return n
end function

function bump(mut x as int, y as int)
    x = x + y
end function

function via(g as function(int) as int)
    bump(xs[1], g(3))
end function

bump(xs[1], churn(7))
print "mut element "; xs[1]
bump(ps[0].x, churn(5))
print "mut field "; ps[0].x
ps[1].y = churn(9)
print "field store "; ps[1].y
boxes[0].items[churn(1)] = 66
print "held element "; boxes[0].items[1]
bump(bx.items[1], churn(1))
print "list in cells "; bx.items[1]
bump(xs[0], ps[0].bumped())
print "method "; xs[0]
via(churn)
print "fn value "; xs[1]
