' A fixed array field passed to a mut T[]: its elements change, its size
' can't: a resize() of it fails when it runs.
IMPORT rows
TYPE Row
    V(2) AS LONG
END TYPE
DIM r AS Row
rows.bump r.V()
PRINT r.V(1)
rows.grow r.V()
