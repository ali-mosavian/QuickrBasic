option dialect quickr
' File.read_into: an array of numbers filled from the file's place,
' its bytes as they lie in memory; the whole elements read
import fs
import text
from fs import File
let codes = "1 2 3 4 254 255 44 1 144 238 254 255 0 0 192 63 9 9"
var bytes = ""
for c in text.split(codes, " ")
    bytes = bytes + text.chr(text.to_int(c) or else 0)
next
print fs.write_text("fsinto.bin", bytes) is Error
let f = File.open("fsinto.bin")
if f is not Error then
    var b = ubyte[4]
    var s = short[2]
    var i = int[1]
    var x = single[1]
    var rest = int[2]
    var empty as int[]
    print f.read_into(b) or else -1; " "; b[0]; " "; b[3]
    print f.read_into(s) or else -1; " "; s[0]; " "; s[1]
    print f.read_into(i) or else -1; " "; i[0]
    print f.read_into(x) or else -1; " "; x[0]
    print f.read_into(empty) or else -1
    print f.read_into(rest) or else -1
end if
