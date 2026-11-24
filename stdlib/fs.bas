' OPTION DIALECT QUICKR
' fs: files, each error a value. A File closes itself when it ends, or at close().
import text

native function file_open(path as string, mode as string) as int
native function file_read(n as int, count as int) as string
native function file_write(n as int, s as string) as int
native function file_seek(n as int, at as int) as int
native function file_size(n as int) as int
native function file_close(n as int)
native function file_list(pattern as string) as string
native function error_text(err as int) as string

' an open file: its number, as OPEN's; 0 closed
public type File
    n as int
end type

function drop(mut f as File)
    close(mut f)
end function

' path opened: mode "r" to read, "w" to write it anew, "a" to add to its end, "rw" both
public function open(path as string, mode as string = "r") as File!
    let n = file_open(path, mode)
    if n < 0 then return Error(-n, f"{path}: {error_text(-n)}")
    return File(n)
end function

' count bytes at most from f's place, fewer at its end
public function read(f as File, count as int) as string
    return file_read(f.n, count)
end function

public function write(f as File, s as string) as bool!
    let st = file_write(f.n, s)
    if st < 0 then return Error(-st, f"write: {error_text(-st)}")
    return true
end function

' f's place to byte at, from 0
public function seek(f as File, at as int) as bool!
    let st = file_seek(f.n, at)
    if st < 0 then return Error(-st, f"seek: {error_text(-st)}")
    return true
end function

public function size(f as File) as int
    return file_size(f.n)
end function

public function close(mut f as File)
    if f.n > 0 then file_close(f.n)
    f.n = 0
end function

' the whole file as a string
public function read_text(path as string) as string!
    let f = open(path)?
    return read(f, size(f))
end function

public function write_text(path as string, s as string) as bool!
    var f = open(path, "w")?
    let st = file_write(f.n, s)
    f.close()
    if st < 0 then return Error(-st, f"{path}: {error_text(-st)}")
    return true
end function

' the names of the files matching pattern, wildcards * and ?
public function list(pattern as string = "*.*") as string[]
    var names = text.split(file_list(pattern), text.chr(10))
    names.pop()
    return names
end function
