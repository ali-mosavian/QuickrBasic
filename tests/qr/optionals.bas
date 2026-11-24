' OPTION DIALECT QUICKR
' T? and none, T! and Error, ??, x?, is narrowing, map get
const EMPTY = 1
const BIG = 2
function half(n as int) as int?
    if n mod 2 <> 0 then return none
    return n div 2
end function
function parse(s as string) as int!
    if len(s) = 0 then return Error(EMPTY, "empty")
    if len(s) > 3 then
        return Error(BIG, f"{s} is too long")
    end if
    return len(s) * 10
end function
function twice(s as string) as int!
    let n = parse(s)?
    return n * 2
end function
function label(s as string) as string!
    let n = twice(s)?
    return f"n={n}"
end function
function show(n as int)
    let h = half(n)
    if h is none then
        print n; " "; "odd"
        return
    end if
    print n; " "; "half"; " "; h + 0
end function
show(4)
show(7)
print half(9) ?? -1; " "; half(10) ?? -1
var o as int? = 5
print o is none; " "; o is not none; " "; o ?? 0
o = none
print o is none; " "; o ?? 42
function report(s as string)
    let r = label(s)
    if r is Error then
        print "error"; " "; r.code; " "; r.message; " "; r.line
        return
    end if
    print "ok"; " "; r
end function
report("")
report("ab")
report("abcdef")
print parse("abc") ?? 0; " "; parse("") ?? -1
var m as int[string]
m["a"] = 1
let g = m.get("a")
let none_ = m.get("b")
print g ?? 0; " "; none_ ?? 0; " "; none_ is none
var names as string[int]
names[3] = "three"
print names.get(3) ?? "?"; " "; names.get(4) ?? "?"
function first(xs as string[]) as string?
    if len(xs) = 0 then return none
    return xs[0]
end function
var xs as string[]
print first(xs) ?? "nothing"
xs.push("hi")
print first(xs) ?? "nothing"
' the branch a test proves sees the T: else of is Error / is none,
' the then of is not none; without, print(r) took an int! and failed
function branches(s as string, n as int)
    let r = parse(s)
    if r is Error then
        print "bad"; " "; r.code
    else
        print "good"; " "; r
    end if
    let h = half(n)
    if h is none then
        print "odd"
    else
        print "half"; " "; h
    end if
    if h is not none then print "again"; " "; h + 1
    if h is not none then
        print "block"; " "; h
    end if
end function
branches("ab", 6)
branches("", 3)
