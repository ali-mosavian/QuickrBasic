# io Module
keys: ~quickr io, ~quickr io.read_line, ~quickr io.at, ~quickr io.color, ~quickr io.clear

The console. print needs no import.

    read_line() as string           a line typed, its Enter left out
    at(row as int, col as int)      the cursor there, each from 1
    color(fg as int, bg as int = 0) the colours of what is printed
    clear()                         the screen cleared

See also [Quickr](quickr index), [text](quickr text)

## Details
The colours are COLOR's, 0 to 15. at() moves the cursor as LOCATE
does.

## Example
    ' OPTION DIALECT QUICKR
    import io
    io.clear()
    io.at(5, 10)
    io.color(14)
    print "Your name?"
    let name = io.read_line()
    print f"Hello, {name}"
