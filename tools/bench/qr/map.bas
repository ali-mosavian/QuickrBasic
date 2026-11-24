defint a-z
const cap = 8192
dim keys&(cap - 1), vals(cap - 1), used(cap - 1)
n = 0
for i = 0 to 3999
k& = i * 7919&
p = (k& xor (k& \ cap)) and (cap - 1)
while used(p) and keys&(p) <> k&
p = (p + 1) and (cap - 1)
wend
if not used(p) then used(p) = -1: keys&(p) = k&: n = n + 1
vals(p) = i
next
sum& = 0
for r = 0 to 4
for i = 0 to 3999
k& = i * 7919&
p = (k& xor (k& \ cap)) and (cap - 1)
while keys&(p) <> k&
p = (p + 1) and (cap - 1)
wend
sum& = sum& + vals(p)
next
next
print n; sum&
