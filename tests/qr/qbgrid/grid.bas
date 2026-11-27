TYPE Map
    C(2, 1) AS LONG
END TYPE

SUB Mark (a() AS LONG)
    a(2, 1) = 8
END SUB

FUNCTION Total& (a() AS LONG)
    t& = 0
    FOR j = 0 TO UBOUND(a, 2)
        FOR i = 0 TO UBOUND(a, 1)
            t& = t& + a(i, j)
        NEXT
    NEXT
    Total& = t&
END FUNCTION
