option dialect quickr
' T or none, T or Error, or else, x?, is narrowing, dict get
const EMPTY = 1
const BIG = 2
function half(n as int) as int or none
    if n mod 2 <> 0 then return none
    return n div 2
end function
function parse(s as string) as int or Error
    if len(s) = 0 then return Error(EMPTY, "empty")
    if len(s) > 3 then
        return Error(BIG, f"{s} is too long")
    end if
    return len(s) * 10
end function
function twice(s as string) as int or Error
    let n = parse(s)?
    return n * 2
end function
function label(s as string) as string or Error
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
print half(9) or else -1; " "; half(10) or else -1
var o as int or none = 5
print o is none; " "; o is not none; " "; o or else 0
o = none
print o is none; " "; o or else 42
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
print parse("abc") or else 0; " "; parse("") or else -1
var m as dict[string, int]
m["a"] = 1
let g = m.get("a")
let none_ = m.get("b")
print g or else 0; " "; none_ or else 0; " "; none_ is none
var names as dict[int, string]
names[3] = "three"
print names.get(3) or else "?"; " "; names.get(4) or else "?"
function first(xs as string[]) as string or none
    if len(xs) = 0 then return none
    return xs[0]
end function
var xs as string[]
print first(xs) or else "nothing"
xs.push("hi")
print first(xs) or else "nothing"
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
' in a type, or binds last: a list of optionals; a function whose result is optional
var opts as (int or none)[]
opts.push(none)
opts.push(4)
print opts[0] or else -1; " "; opts[1] or else -1
var hf as function(int) as int or none = half
print hf(8) or else -1; " "; hf(7) or else -1
' or else binds below or: a bool or none's value or false
var flag as bool or none = none
print flag or else true or false
