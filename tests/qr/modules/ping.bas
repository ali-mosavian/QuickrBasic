' OPTION DIALECT QUICKR
' a cycle: ping and pong call each other
import pong
function ping(n as int) as int
    return 0 if n = 0 else pong.pong(n - 1) + 1
end function
var first = start("ping")
private function start(s as string) as string
    print "init"; " "; s; " "; pong.second
    return s
end function
