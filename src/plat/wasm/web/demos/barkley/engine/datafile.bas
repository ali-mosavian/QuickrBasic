' A data file read: its LONGs and its strings.
defint A-Z
option explicit

' A data file: its name, what to say when it is not there,
' and its number while it is open.
type DataFile
    path as string
    lack as string
    num as integer
end type

' df named path; lack is said when it is not there.
sub NameData (df as DataFile, path as string, lack as string)
    df.path = path
    df.lack = lack
end sub

' df open, at its start; when it is not there the program
' ends, saying df.lack.
sub OpenData (df as DataFile)
    df.num = freefile
    open df.path for binary as #df.num

    if lof(df.num) = 0 then
        close #df.num
        kill df.path
        screen 0
        print df.lack
        end
    end if
end sub

' df's next LONG.
function Word& (df as DataFile)
    dim v as long

    get #df.num, , v
    Word& = v
end function

' df's next string: its length, then its characters.
function Chars$ (df as DataFile)
    dim s as string

    s = space$(Word&(df))
    if len(s) then get #df.num, , s
    Chars$ = s
end function
