defint a-z
dim f(999) as single
for i = 0 to 999
f(i) = (i * 37) mod 101
next
lo! = f(0)
hi! = f(0)
for k = 1 to 300
for i = 1 to 999
if f(i) < lo! then lo! = f(i)
if f(i) > hi! then hi! = f(i)
next
next
print lo!
print hi!
