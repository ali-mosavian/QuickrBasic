# fs Module
keys: ~quickr fs, ~quickr fs.File, ~quickr File.open, ~quickr fs.read_text, ~quickr fs.write_text, ~quickr fs.list

Files, each error a value.

    File.open(path, mode = "r") as File or Error
                                     "r", "w" anew, "a" to its end, "rw"
    f.read(count) as string          count bytes at most from its place
    f.write(s) as bool or Error      s at its place
    f.seek(at) as bool or Error      its place to byte at, from 0
    f.size() as int                  its bytes
    f.close()                        closed
    read_text(path) as string or Error  the whole file; past 32767 bytes an Error
    write_text(path, s) as bool or Error  the file made s
    list(pattern = "*.*") as list[string]  the names matching * and ?

See also [Quickr](quickr index), [os](quickr os)

## Details
A File closes itself when its owner ends; its number is private.
An error is returned, not raised: test it with is Error, or pass it
on with ?.

## Example
    option dialect quickr
    import fs
    let r = fs.read_text("notes.txt")
    if r is Error then
        print r.message
    else
        print r
    end if
