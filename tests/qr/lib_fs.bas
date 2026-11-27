option dialect quickr
' fs: whole files, a File's read, write and seek, errors as values
import fs
from fs import File
import text
print fs.write_text("libfs.txt", "line one\nline two") is Error
let r = fs.read_text("libfs.txt")
if r is not Error then print len(r); " "; r[..8]
let m = fs.read_text("missing.txt")
print m is Error; " "; m.code; " "; m.message
let f = File.open("libfs.bin", "rw")
if f is not Error then
    print f.write("abcdef") is Error
    print f.seek(2) is Error
    print f.read(3); " "; f.read(10); " "; f.size()
end if
function unclosed(path as string) as bool or Error
    let f = File.open(path, "w")?
    return f.write("written, closed by its drop")?
end function
function closed(path as string) as bool or Error
    var f = File.open(path, "w")?
    let ok = f.write("closed twice")?
    f.close()
    f.close()
    return ok
end function
print unclosed("libfs.drp") or else false; " "; fs.read_text("libfs.drp") or else ""
print closed("libfs.drp") or else false; " "; fs.read_text("libfs.drp") or else ""
let names = fs.list("libfs.*")
print len(names); " "; text.upper(names[0])
let bad = File.open("x.txt", "q")
print bad.message
' a file past a string's length: an Error, not the file cut short
let big = File.open("libfs.big", "w")
if big is not Error then
    let half = f"{0:>30000}"
    print big.write(half) is Error; " "; big.write(half) is Error
end if
let whole = fs.read_text("libfs.big")
print whole is Error; " "; whole.message
