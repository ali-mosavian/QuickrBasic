TYPE Pt
    X AS LONG
END TYPE
CONST TOP = 100
DIM SHARED made AS LONG
made = 5
PRINT "geom runs"
SUB Scale (p AS Pt, k AS LONG)
    p.X = p.X * k
END SUB
FUNCTION Count& ()
    Count& = made
END FUNCTION
