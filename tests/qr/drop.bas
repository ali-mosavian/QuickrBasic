' OPTION DIALECT QUICKR
' drop: run when an owner of a value with drop ends
type Res
    name as string
end type

function drop(mut r as Res)
    print "drop"; " "; r.name
end function

type Pair
    a as Res
    b as Res
    n as int
end type

type Pool
    items as Res[]
    first as Res or none
end type

function make(name as string) as Res
    return Res(name)
end function

function early(stop as bool) as int
    let x = Res("x")
    if stop then return 1
    let y = Res("y")
    return 2
end function

function kept() as Res
    let k = Res("k")
    let tmp = Res("tmp")
    return k
end function

function check(fail as bool) as int or Error
    if fail then return Error(1, "no")
    return 5
end function

function tried(fail as bool) as int or Error
    let t = Res("t")
    let v = check(fail)?
    let u = Res("u")
    return v
end function

function in_loop()
    for i in 0..3
        let r = Res(f"loop{i}")
    next
    print "loop done"
end function

function maybe_move(go as bool)
    let a = Res(f"moved {go}")
    if go then
        let b = a
        print "in"; " "; b.name
    end if
    print "out"
end function

function pool() as int
    var p as Pool
    p.items.push(Res("i0"))
    p.items.push(Res("i1"))
    p.items.push(Res("i2"))
    p.items.remove(1)
    p.items.pop()
    p.first = make("first")
    return len(p.items)
end function

function pairs()
    var p = Pair(Res("pa"), Res("pb"), 1)
    p.a = Res("pa2")
    var q = Pair(b: Res("qb"))
    q = Pair(Res("qa2"), Res("qb2"), 2)
    let r, s = (Res("r"), Res("s"))
end function

function either(c as bool)
    let a = Res("a")
    let b = Res("b")
    let g = a if c else b
    print "g"; " "; g.name
end function

print early(true)
print early(false)
let got = kept()
print "got"; " "; got.name
print tried(true) is Error
print tried(false) or else 0
in_loop()
maybe_move(true)
maybe_move(false)
print pool()
pairs()
either(true)
either(false)
var w as Res
w = make("w")
make("discarded")
let moved = got
var list as Res[]
list.push(Res("l0"))
list.push(moved)
for i in 0..2
    let each = Res(f"top{i}")
next
print "end"
