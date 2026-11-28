option dialect quickr
' generic records: one copy of the type and its methods per types given
import shelf
from shelf import Shelf
type Vec
    x as int
    y as int
end type
type Stack[T]
    items as list[T]

    fn push(mut self, x as T)
        self.items.push(x)
    end fn

    fn pop(mut self) as T
        return self.items.pop()
    end fn

    fn top(self) as T or none
        if len(self.items) = 0 then return none
        return self.items[len(self.items) - 1]
    end fn

    fn size(self) as int
        return len(self.items)
    end fn

    fn empty() as Stack[T]
        var s as Stack[T]
        return s
    end fn

    fn map[U](self, f as function(T) as U) as Stack[U]
        var out as Stack[U]
        for x in self.items
            out.push(f(x))
        next
        return out
    end fn

    fn sorted(self) as list[T]
        var a = self.items
        a.sort()
        return a
    end fn

    fn text(self) as string
        return f"a stack of {len(self.items)}"
    end fn
end type
type Pair[A, B]
    first as A
    second as B
end type
type Guard[T]
    v as T

    fn drop(mut self)
        print f"dropped {self.v}"
    end fn
end type
function total[T](s as Stack[T]) as T
    var t as T
    for x in s.items
        t += x
    next
    return t
end function
function guarded()
    let g = Guard("a")
    let h = Guard(2)
    print "in"; " "; g.v; h.v
end function
var s as Stack[int]
s.push(3)
s.push(4)
print s.size(); " "; s.top(); " "; s; " "; s.sorted()[0]
var t = Stack[string]()
print t.top(); " "; t.size()
var vs as Stack[Vec]
vs.push(Vec(1, 2))
vs.push(Vec(3, 4))
print vs.items[1].x; " "; vs.size()
var ls as Stack[list[int]]
var row as list[int]
row.push(5)
ls.push(row)
ls.items[0].push(6)
print len(ls.items[0]); " "; len(row)
var ss as Stack[Stack[int]]
var inner = Stack[int].empty()
inner.push(7)
ss.push(inner)
ss.items[0].push(8)
print ss.items[0].size(); " "; ss.size()
let st = ss.pop()
print total(st)
let halves = st.map(fn(x) x / 2)
print total(halves); " "; halves.top()
let p = Pair(st, "s")
print p.first.size(); " "; p.second
let q = Pair[double, int](2, 3)
print q.first / 4; " "; q.second
let mk = Stack[string].empty
var e = mk()
e.push("hi")
print e.pop()
var copy = vs
copy.push(Vec(0, 0))
print vs.size(); " "; copy.size()
guarded()
var sh as shelf.Shelf[string]
sh.put("cup")
var sh2 as Shelf[int]
sh2.put(1)
sh2.put(2)
print sh.count_of(); " "; sh2.count_of(); " "; sh.things[0]
