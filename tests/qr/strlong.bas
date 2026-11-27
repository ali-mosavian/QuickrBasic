option dialect quickr
' A string grown past its 32767 bytes, as text.Builder's text() does:
' String too long, not "Unprintable error", its number's message missing
var s = ""
for i in 0..5
    s = s + f"{i:>8000}"
next
print len(s)
