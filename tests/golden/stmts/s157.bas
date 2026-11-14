TYPE T
a AS INTEGER
b AS STRING * 4
c AS DOUBLE
END TYPE
DIM r AS T, q AS T
r.a = 1
r.b = "x"
r.c = 1
r.a = "x"
r = q
r.b = 1
SWAP r.a, x
INPUT r.a
r.c = 1 +
PRINT r.a; r.b
