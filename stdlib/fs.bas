option dialect quickr
' fs: files, each error a value. A File closes itself when it ends, or at close().
import text

private native function file_open(path as string, mode as string) as int
private native function file_read(n as int, count as int) as string
private native function file_write(n as int, s as string) as int
private native function file_seek(n as int, at as int) as int
private native function file_size(n as int) as int
private native function file_close(n as int)
private native function file_list(pattern as string) as string
private native function error_text(err as int) as string

' an open file: its number, as OPEN's; 0 closed
type File
    private n as int

    ' path opened: mode "r" to read, "w" to write it anew, "a" to add to its end, "rw" both
    fn open(path as string, mode as string = "r") as File or Error
        let n = file_open(path, mode)
        if n < 0 then return Error(-n, f"{path}: {error_text(-n)}")
        return File(n)
    end fn

    ' count bytes at most from the file's place, fewer at its end
    fn read(self, count as int) as string
        return file_read(self.n, count)
    end fn

    fn write(self, s as string) as bool or Error
        let st = file_write(self.n, s)
        if st < 0 then return Error(-st, f"write: {error_text(-st)}")
        return true
    end fn

    ' the file's place to byte at, from 0
    fn seek(self, at as int) as bool or Error
        let st = file_seek(self.n, at)
        if st < 0 then return Error(-st, f"seek: {error_text(-st)}")
        return true
    end fn

    fn size(self) as int
        return file_size(self.n)
    end fn

    fn close(mut self)
        if self.n > 0 then file_close(self.n)
        self.n = 0
    end fn

    fn drop(mut self)
        self.close()
    end fn
end type

' the whole file as a string
function read_text(path as string) as string or Error
    let f = File.open(path)?
    return f.read(f.size())
end function

function write_text(path as string, s as string) as bool or Error
    var f = File.open(path, "w")?
    let st = file_write(f.n, s)
    f.close()
    if st < 0 then return Error(-st, f"{path}: {error_text(-st)}")
    return true
end function

' the names of the files matching pattern, wildcards * and ?
function list(pattern as string = "*.*") as list[string]
    var names = text.split(file_list(pattern), text.chr(10))
    names.pop()
    return names
end function
