FUNCTION Total& (a() AS LONG)
    FOR i = LBOUND(a) TO UBOUND(a)
        t& = t& + a(i)
    NEXT
    Total& = t&
END FUNCTION

FUNCTION Top& (a() AS LONG)
    Top& = UBOUND(a)
END FUNCTION

SUB Grow (a() AS LONG)
    REDIM PRESERVE a(0 TO 2) AS LONG
    a(2) = 9
END SUB

FUNCTION First% (a() AS SHORT)
    First% = a(0)
END FUNCTION

FUNCTION Joined$ (a() AS STRING)
    FOR i = 0 TO UBOUND(a)
        j$ = j$ + a(i)
    NEXT
    Joined$ = j$
END FUNCTION
