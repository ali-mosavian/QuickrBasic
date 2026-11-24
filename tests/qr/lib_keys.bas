' OPTION DIALECT QUICKR
' keys: codes; nothing waits headless, and no mouse
import keys
from keys import Key
print Key.escape.code(); " "; Key.up.code(); " "; Key.q.code(); " "; Key.space.code(); " "; Key.q
print keys.next(); " "; keys.pressed(Key.b); " "; keys.down(Key.z)
let m = keys.mouse()
print m.x; " "; m.y; " "; m.left; " "; m.right; " "; keys.clicked()
