' OPTION DIALECT QUICKR
' fs: whole files, a File's read, write and seek, errors as values
import fs
import text
print fs.write_text("libfs.txt", "line one\nline two") is Error
let r = fs.read_text("libfs.txt")
if r is not Error then print len(r); " "; r[..8]
let m = fs.read_text("missing.txt")
print m is Error; " "; m.code; " "; m.message
let f = fs.open("libfs.bin", "rw")
if f is not Error then
    print fs.write(f, "abcdef") is Error
    print fs.seek(f, 2) is Error
    print fs.read(f, 3); " "; fs.read(f, 10); " "; fs.size(f)
end if
function unclosed(path as string) as bool or Error
    let f = fs.open(path, "w")?
    return fs.write(f, "written, closed by its drop")?
end function
function closed(path as string) as bool or Error
    var f = fs.open(path, "w")?
    let ok = fs.write(f, "closed twice")?
    fs.close(mut f)
    fs.close(mut f)
    return ok
end function
print unclosed("libfs.drp") or else false; " "; fs.read_text("libfs.drp") or else ""
print closed("libfs.drp") or else false; " "; fs.read_text("libfs.drp") or else ""
let names = fs.list("libfs.*")
print len(names); " "; text.upper(names[0])
let bad = fs.open("x.txt", "q")
print bad.message
