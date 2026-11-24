defint a-z
declare function largest(a() as integer)
dim a(999) as integer
for i = 0 to 999
a(i) = (i * 7919&) mod 1000
next
t& = 0
for r = 0 to 299
t& = t& + largest(a())
next
print t&
function largest(a() as integer)
best = a(0)
for i = 0 to ubound(a)
if a(i) > best then best = a(i)
next
largest = best
end function
