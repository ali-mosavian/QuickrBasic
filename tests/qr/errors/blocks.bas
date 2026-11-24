' OPTION DIALECT QUICKR
' blocks closed wrongly; break and continue outside a loop; return outside a function, one-line too
if true then
    print(1)
end while
end if
break
continue
return
if true then return
function f()
    function g()
    end function
end function
while true
    print(1)
end fn
do
