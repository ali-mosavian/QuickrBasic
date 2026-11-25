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
call stp(p, v)
p.x = p.x mod 1000
next
print p.x; p.y
sub stp (p as Vec, v as Vec)
p.x = p.x + v.x
p.y = p.y + v.y
end sub
