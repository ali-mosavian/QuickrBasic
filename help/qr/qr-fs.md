# fs Module
keys: ~quickr fs, ~quickr fs.File, ~quickr fs.open, ~quickr fs.read, ~quickr fs.write, ~quickr fs.seek, ~quickr fs.size, ~quickr fs.close, ~quickr fs.read_text, ~quickr fs.write_text, ~quickr fs.list

Files, each error a value.

    open(path, mode = "r") as File!  "r", "w" anew, "a" to its end, "rw"
    read(f, count) as string         count bytes at most from its place
    write(f, s) as bool!             s at its place
    seek(f, at) as bool!             its place to byte at, from 0
    size(f) as int                   its bytes
    close(mut f)                     closed
    read_text(path) as string!       the whole file
    write_text(path, s) as bool!     the file made s
    list(pattern = "*.*") as string[]  the names matching * and ?

See also [Quickr](quickr index), [os](quickr os)

## Details
A File closes itself when its owner ends. An error is returned, not
raised: test it with is Error, or pass it on with ?.

## Example
    ' OPTION DIALECT QUICKR
    import fs
    let r = fs.read_text("notes.txt")
    if r is Error then
        print r.message
    else
        print r
    end if
