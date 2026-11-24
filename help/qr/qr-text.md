# text Module
keys: ~quickr text, ~quickr text.find, ~quickr text.find_from, ~quickr text.upper, ~quickr text.lower, ~quickr text.trim, ~quickr text.chr, ~quickr text.code, ~quickr text.hex, ~quickr text.number, ~quickr text.split, ~quickr text.join, ~quickr text.Builder, ~quickr text.add, ~quickr text.text

Searching, cases, splitting and joining strings.

    find(s, sub) as int            sub's place in s, from 0; -1 none
    find_from(s, sub, start) as int  the same from start on
    upper(s), lower(s) as string   the letters' case changed
    trim(s) as string              the spaces at its ends cut
    chr(code) as string            the character of code
    code(s) as int                 its first character's code
    hex(n) as string               n in hexadecimal
    number(s) as double            the number s spells
    split(s, sep) as string[]      the parts between seps
    join(parts, sep) as string     the parts, sep between them

See also [Quickr](quickr index), [io](quickr io)

## Details
A Builder builds a string in parts: add(mut b, s) each, then text(b)
takes the whole. split of a string without sep gives one part, s.

## Example
    ' OPTION DIALECT QUICKR
    import text
    let words = text.split("a,b,c", ",")
    print text.join(words, " ")
    print text.upper("quickr")
