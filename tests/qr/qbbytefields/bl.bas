TYPE BL
    A(3) AS BYTE
    G() AS BYTE
    One AS BYTE
    Sea AS LONG
END TYPE
SUB Fill (x AS BL)
    x.A(2) = 201: x.One = 254: x.Sea = 3
    REDIM x.G(2): x.G(2) = 9
END SUB
