option dialect quickr
' character constants: a string constant indexed by a constant, a ubyte
const ZERO = "0"[0]
const NL = "a\n"[1]
const TAB = "\t"[0]
print ZERO; " "; NL; " "; TAB; " "; "\\"[0]; " "; "\""[0]
function digit(c as ubyte) as bool
    return "0"[0] <= c <= "9"[0]
end function
let s = "a1"
print digit(s[0]); " "; digit(s[1])
select case s[1]
    case "0"[0] to "9"[0]
        print "a digit"
    case "a"[0]
        print "an a"
    case else
        print "?"
end select
const WORD = "quick"
print WORD[4]; " "; ubyte("A"[0] + 1)
