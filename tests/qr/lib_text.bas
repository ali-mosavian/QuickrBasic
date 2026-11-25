' OPTION DIALECT QUICKR
' text: INSTR, UCASE$, LTRIM$ .. as entries; split, join, Builder in Quickr
import text
let s = "  Hello, World  "
print text.find(s, "o"); " "; text.find(s, "zz"); " "; text.find_from(s, "o", 7); " "; text.find(s, "")
print f"[{text.trim(s)}] [{text.upper(s)}] [{text.lower(s)}]"
print text.chr(65); " "; text.code("A"); " "; text.hex(255); " "; text.number("2.5") * 2
let parts = text.split("a,b,,c", ",")
print len(parts); " "; parts[0]; " "; parts[2] = ""; " "; parts[3]
print text.join(parts, "-"); " "; text.join(text.split("x", ","), "+")
var b as text.Builder
b.add("ab")
b.add("cd")
print b.text(); " "; b
function read(s as string) as string
    let r = text.to_int(s)
    if r is Error then return f"E{r.code}"
    return f"{r}"
end function
print read("42"); " "; read("-17"); " "; read("007"); " "; read("-0")
print read("2147483647"); " "; read("-2147483648"); " "; read("2147483648"); " "; read("-2147483649"); " "; read("99999999999999999999")
print read(""); " "; read("-"); " "; read("12a"); " "; read(" 1"); " "; read("1 "); " "; read("+1"); " "; read("1.5")
