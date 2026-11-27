IMPORT leaf
FUNCTION Twice& (x AS leaf.Leaf)
    Twice& = x.v * 2 + leaf.Mk(1).v - 1
END FUNCTION
