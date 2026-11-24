defint a-z
dim a(999) as integer
for i = 0 to 999
a(i) = (i * 7919&) mod 1000
next
t& = 0
for r = 0 to 299
s& = 0
for i = 0 to 999
if a(i) mod 3 = 0 then s& = s& + a(i) * 2
next
t& = t& + s&
next
print t&
