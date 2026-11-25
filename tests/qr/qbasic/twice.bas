' OPTION DIALECT QUICKR
' a Quickr module importing a QBasic one its main imports too: its code runs once
import shapes

public function quad(x as int) as int
    return shapes.twice(shapes.twice(x))
end function
