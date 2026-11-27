' A fixed array field can't grow: no mut list takes it.
IMPORT rows
TYPE Row
    V(2) AS LONG
END TYPE
DIM r AS Row
rows.grow r.V()
