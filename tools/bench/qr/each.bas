defint a-z
declare function total& (a() as long)
dim a(999) as long
for i = 0 to 999
a(i) = i mod 500
next
print total&(a())
function total& (a() as long)
s& = 0
for k = 0 to 3999
for i = 0 to 999
s& = s& + a(i)
next
next
total& = s&
end function
