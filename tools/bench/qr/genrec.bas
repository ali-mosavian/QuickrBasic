defint a-z
type Acc
total as long
n as long
end type
dim a as Acc
for i = 0 to 1999999
call add(a, i mod 7)
a.total = a.total mod 1000
next
print a.total; a.n
sub add (a as Acc, x as long)
a.total = a.total + x
a.n = a.n + 1
end sub
