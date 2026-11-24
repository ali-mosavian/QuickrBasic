' OPTION DIALECT QUICKR
' one-line if and block if / elseif / else
function grade(n as int) as string
    if n >= 90 then
        return "a"
    elseif n >= 80 then
        return "b"
    elseif n >= 70 then
        return "c"
    else
        return "f"
    end if
end function
print grade(95); " "; grade(85); " "; grade(75); " "; grade(10)
var x = 3
if x > 2 then print "big"
if x > 5 then print "huge"
if x = 3 then x = 4
print x
if x = 4 then
    print "four"
end if
if x = 5 then
    print "five"
else
    print "not five"
end if
if x < 0 then
    print "neg"
elseif x < 10 then
    if x mod 2 = 0 then
        print "small even"
    else
        print "small odd"
    end if
end if
' else if is elseif; a one-line if takes an else
function sign(n as int) as string
    if n < 0 then
        return "-"
    else if n = 0 then
        return "0"
    else
        return "+"
    end if
end function
print sign(-5); " "; sign(0); " "; sign(5)
if x = 4 then print "yes" else print "no"
if x = 5 then print "yes" else print "no"
if x > 0 then x = 1 else x = -1
print x
function pick(a as bool) as int
    if a then return 1 else return 2
end function
print pick(true); " "; pick(false)
function shown(v as int?) as string
    if v is none then return "none" else return f"{v + 1}"
end function
print shown(none); " "; shown(41)
for k in 0..4
    if k mod 2 = 0 then continue else print "odd"; " "; k
next
