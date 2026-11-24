defint a-z
dim a(999)
for i = 0 to 999
a(i) = i * 3
next
s = 0
for k = 0 to 2999
for i = 0 to 999
s = s + a(i) mod 7
next
next
print s
