defint a-z
declare function sumwhere&(a() as integer, lim)
dim a(999) as integer
for i = 0 to 999
a(i) = (i * 7919&) mod 1000
next
t& = 0
lim = 500
for r = 0 to 299
t& = t& + sumwhere&(a(), lim)
next
print t&
function sumwhere&(a() as integer, lim)
s& = 0
for i = 0 to ubound(a)
if a(i) > lim then s& = s& + a(i)
next
sumwhere& = s&
end function
