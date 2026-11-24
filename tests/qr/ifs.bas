' OPTION DIALECT QUICKR
' one-line if and block if / elseif / else
fn grade(n as int) as string
    if n >= 90 then
        return "a"
    elseif n >= 80 then
        return "b"
    elseif n >= 70 then
        return "c"
    else
        return "f"
    end if
end fn
print(grade(95), grade(85), grade(75), grade(10))
var x = 3
if x > 2 then print("big")
if x > 5 then print("huge")
if x = 3 then x = 4
print(x)
if x = 4 then
    print("four")
end if
if x = 5 then
    print("five")
else
    print("not five")
end if
if x < 0 then
    print("neg")
elseif x < 10 then
    if x mod 2 = 0 then
        print("small even")
    else
        print("small odd")
    end if
end if
