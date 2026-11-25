defint a-z
s$ = "the quick brown fox jumps over the lazy dog"
n& = 0
for r = 0 to 19999
for i = 1 to len(s$)
if asc(mid$(s$, i, 1)) = 111 then n& = n& + 1
next
next
print n&; mid$(s$, 5, 5)
