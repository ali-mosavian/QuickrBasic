t& = 0
for s& = 1 to 2999
x& = s&
while x& <> 1
if x& mod 2 = 0 then
x& = x& \ 2
else
x& = 3 * x& + 1
end if
t& = t& + x& mod 7
wend
next
print t&
