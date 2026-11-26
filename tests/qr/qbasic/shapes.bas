' A QBasic module a Quickr program imports: its code runs at the first import
CONST SIDES = 4, DIAGS = SIDES * 2
CONST SHARE! = .5, TAU# = 6.283185307179586#, TITLE$ = "shapes"
IMPORT counts
TYPE Rect
    W AS SINGLE
    H AS SINGLE
    Label AS STRING
END TYPE
TYPE Pt
    X AS LONG
    Y AS INTEGER
    S AS SHORT
END TYPE
DIM SHARED made AS LONG
made = 7
PRINT "shapes starts"

FUNCTION Area! (r AS Rect)
    Area! = r.W * r.H
END FUNCTION

SUB Grow (r AS Rect, by AS SINGLE)
    r.W = r.W + by
    r.H = r.H + by
    r.Label = r.Label + "+"
END SUB

SUB Move (p AS Pt, dx AS LONG)
    p.X = p.X + dx
    p.Y = p.Y + 1
    p.S = p.S - 1
END SUB

SUB Bump (n AS INTEGER, l AS LONG, s AS SHORT)
    n = n + 1
    l = l * 2
    s = s - 1
END SUB

FUNCTION Twice& (x AS LONG)
    Twice& = counts.Plus&(x, x)
END FUNCTION

FUNCTION Greet$ (who AS STRING)
    who = who + "!"
    Greet$ = "hello " + who
END FUNCTION

FUNCTION Half# (d AS DOUBLE)
    Half# = d / 2
END FUNCTION

FUNCTION Total& ()
    Total& = made
END FUNCTION

FUNCTION Square (s AS SINGLE) AS Rect
    DIM r AS Rect
    r.W = s
    r.H = s
    r.Label = "square"
    Square = r
END FUNCTION

PRIVATE SUB Hidden
END SUB
