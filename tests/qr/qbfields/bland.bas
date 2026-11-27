' A QBasic module whose TYPEs hold arrays: one fixed from 0, a dynamic one, a 2-D one
TYPE BLand
    Heights(7) AS LONG
    G() AS LONG
    Tags() AS STRING
    Sea AS INTEGER
END TYPE
TYPE Grid
    Cells(2, 2) AS LONG
END TYPE
SUB FillB (b AS BLand)
    b.Heights(3) = 9: b.Sea = 4
    REDIM b.G(0 TO 2): b.G(2) = 5
END SUB
FUNCTION SumB& (b AS BLand)
    FOR i = 0 TO UBOUND(b.G): s& = s& + b.G(i): NEXT
    SumB& = s& + b.Heights(3)
END FUNCTION
