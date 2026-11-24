defint a-z
type Vec
x as long
y as long
end type
declare function walk& ()
print walk&
function walk&
dim p as Vec
for i& = 0 to 9999999
p.x = p.x + 3
p.y = (p.y + p.x) mod 997
next
walk& = p.x + p.y
end function
