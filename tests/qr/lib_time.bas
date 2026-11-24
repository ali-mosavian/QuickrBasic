' OPTION DIALECT QUICKR
' time: now()'s shape, a Clock's first tick 0, sleep's wait
import time
let n = time.now()
print len(n); " "; n[4..5]; " "; n[7..8]; " "; n[10..11]; " "; n[13..14]
var c = time.Clock()
print time.tick(mut c)
time.sleep(0.2)
let dt = time.tick(mut c)
print dt >= 0.1 and dt < 2; " "; time.ticks() >= 0
