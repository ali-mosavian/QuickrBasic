defint a-z
s$ = "the quick brown fox jumps over the lazy dog"
n& = 0
for r& = 0 to 49999
if instr(s$, "lazy") > 0 then n& = n& + 1
next
print n&
