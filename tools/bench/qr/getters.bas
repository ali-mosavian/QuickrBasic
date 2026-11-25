defint a-z
type Vec
x as long
y as long
end type
dim p as Vec
dim v as Vec
v.x = 3
v.y = 1
for i = 0 to 1999999
p.x = i mod 100
p.y = i mod 7
s = (s + p.x * v.x + p.y * v.y) mod 1000
next
print s
