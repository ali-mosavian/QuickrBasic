' OPTION DIALECT QUICKR
' blocks closed wrongly; break and continue outside a loop; return outside a fn, one-line too
if true then
    print(1)
end while
end if
break
continue
return
if true then return
fn f()
    fn g()
    end fn
end fn
while true
    print(1)
end fn
do
