defint a-z
n = 0
for i = 0 to 3999
redim preserve b(n)
b(n) = i
n = n + 1
next
print n; b(3999)
