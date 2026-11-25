' OPTION DIALECT QUICKR
import ping
function pong(n as int) as int
    return 0 if n = 0 else ping.ping(n - 1) + 10
end function
var second = "pong"
