' OPTION DIALECT QUICKR
' a cycle: ping and pong call each other
import pong
public fn ping(n as int) as int
    return 0 if n = 0 else pong.pong(n - 1) + 1
end fn
public var first = start("ping")
fn start(s as string) as string
    print("init", s, pong.second)
    return s
end fn
