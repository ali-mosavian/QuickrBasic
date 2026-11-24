' OPTION DIALECT QUICKR
' os: the program's command line and environment
import text

native fn command() as string
public native fn env(name as string) as string
' ends the program, files closed; END has no exit code
public native fn exit()
' runs cmd in the command interpreter, as SHELL does
public native fn run(cmd as string)

' the words after the program's name on its command line
public fn args() as string[]
    var words as string[]
    for w in text.split(command(), " ")
        if len(w) > 0 then words.push(w)
    end for
    return words
end fn
