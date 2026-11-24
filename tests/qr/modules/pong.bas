' OPTION DIALECT QUICKR
import ping
public function pong(n as int) as int
    return 0 if n = 0 else ping.ping(n - 1) + 10
end function
public var second = "pong"
