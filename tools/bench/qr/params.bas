defint a-z
type Acc
n as long
s as long
end type
declare sub walk (a as Acc)
dim a as Acc
call walk(a)
print a.n; a.s
sub walk (a as Acc)
for i& = 0 to 9999999
a.n = a.n + 3
a.s = (a.s + a.n) mod 997
next
end sub
