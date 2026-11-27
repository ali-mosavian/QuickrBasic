defint a-z
dim f(999) as single
for i = 0 to 999
f(i) = i - 500
next
for k = 1 to 300
for i = 0 to 999
s! = abs(f(i)) + 1
next
next
print s!
