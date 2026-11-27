' A QBasic program calls Quickr functions and uses its records:
' mut is BYREF, bool is INTEGER, a record result is a TYPE's.
IMPORT geo
FROM geo IMPORT area, Spot, SIDES, TITLE
PRINT "main"
PRINT geo.count()
PRINT area(2, 3); geo.half(5)
DIM p AS Spot
p.x = 2: p.y = 1.5: p.tag = "p"
geo.scale p, 3
PRINT p.x; p.y; p.tag
PRINT geo.norm(p)
DIM n AS LONG, s AS SHORT, b AS INTEGER
n = 5: s = 4: b = 0
geo.bump n, s, b
PRINT n; s; b
PRINT geo.greet("bob")
PRINT geo.isbig(11); geo.isbig(1)
DIM q AS Spot
q = geo.make(7)
PRINT q.x; q.y; q.tag
' its consts CONSTs, in a TYPE's size and a SUB too
PRINT SIDES; geo.WIDE; geo.SHARE; geo.TAU; geo.SET; geo.DIAGS
PRINT TITLE
DIM row(SIDES) AS INTEGER
PRINT UBOUND(row)
show
ON ERROR GOTO oops
PRINT geo.quot(7, 2)
PRINT geo.quot(1, 0)
PRINT "after"
ON ERROR GOTO 0
PRINT geo.quot(1, 0)
END
oops:
PRINT "caught"; ERR
RESUME NEXT
SUB show
PRINT SIDES * 10; TITLE
END SUB
