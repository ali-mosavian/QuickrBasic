type F
    hp as integer
end type
function live (fs() as F) as integer
    dim i as integer
    dim n as integer
    for i = 0 to ubound(fs)
        if fs(i).hp > 0 then n = n + 1
    next
    live = n
end function
