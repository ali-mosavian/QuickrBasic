defint a-z
s = 0
for i = 0 to 2999999
select case i mod 10
case 1, 2
s = s + 1
case 3 to 5
s = s + 2
case is > 7
s = s + 3
case else
s = s + 4
end select
next
print s
