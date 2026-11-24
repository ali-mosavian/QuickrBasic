defint a-z
t# = 0
for i& = 0 to 199999
t# = t# + sqr(cdbl(i&))
next
print fix(t#)
