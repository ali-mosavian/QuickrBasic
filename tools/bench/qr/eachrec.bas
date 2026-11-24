defint a-z
type Pt
x as long
y as long
end type
declare function total& (a() as Pt)
dim a(999) as Pt
for i = 0 to 999
a(i).x = i mod 500
a(i).y = i mod 7
next
print total&(a())
function total& (a() as Pt)
s& = 0
for k = 0 to 3999
for i = 0 to 999
s& = s& + a(i).x + a(i).y
next
next
total& = s&
end function
