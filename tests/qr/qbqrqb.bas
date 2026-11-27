' QBasic imports a Quickr module that imports QBasic ones: geom,
' which QBasic imports too, its code run once and its TYPE one
' type; tally, read only for the Quickr module.
IMPORT mid
IMPORT geom
DIM p AS geom.Pt
p.X = 2
PRINT mid.far(p); p.X
