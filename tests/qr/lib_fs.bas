' OPTION DIALECT QUICKR
' fs: whole files, a File's read, write and seek, errors as values
import fs
import text
print(fs.write_text("libfs.txt", "line one\nline two") is Error)
let r = fs.read_text("libfs.txt")
if r is not Error then print(len(r), r[..8])
let m = fs.read_text("missing.txt")
print(m is Error, m.code, m.message)
let f = fs.open("libfs.bin", "rw")
if f is not Error then
    print(f.write("abcdef") is Error)
    print(f.seek(2) is Error)
    print(f.read(3), f.read(10), f.size())
end if
fn unclosed(path as string) as bool!
    let f = fs.open(path, "w")?
    return f.write("written, closed by its drop")?
end fn
fn closed(path as string) as bool!
    var f = fs.open(path, "w")?
    let ok = f.write("closed twice")?
    f.close()
    f.close()
    return ok
end fn
print(unclosed("libfs.drp") ?? false, fs.read_text("libfs.drp") ?? "")
print(closed("libfs.drp") ?? false, fs.read_text("libfs.drp") ?? "")
let names = fs.list("libfs.*")
print(len(names), text.upper(names[0]))
let bad = fs.open("x.txt", "q")
print(bad.message)
