defint a-z
type Narrow
s as integer
t as integer
end type
declare function walk& ()
print walk&
function walk&
dim p as Narrow
for i& = 0 to 9999999
p.s = (p.s + 7) mod 1001
p.t = (p.t + p.s) mod 997
next
walk& = p.s + p.t
end function
