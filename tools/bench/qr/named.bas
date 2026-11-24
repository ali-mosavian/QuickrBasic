defint a-z
type Named
x as long
label as string
y as long
end type
declare function walk& ()
print walk&
function walk&
dim p as Named
for i& = 0 to 9999999
p.x = p.x + 3
p.y = (p.y + p.x) mod 997
next
walk& = p.x + p.y
end function
