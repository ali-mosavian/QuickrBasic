defint a-z
s$ = "abc"
n& = 0
for i = 0 to 199999
t$ = "<" + ltrim$(str$(i)) + ":" + s$ + ":" + ltrim$(str$(i mod 7)) + ":" + s$ + ">"
n& = n& + len(t$)
next
print n&
