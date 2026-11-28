' Big letters for the title and the notes: 5 by 7 glyphs,
' each lit bit a square, drawn by rule.
option explicit

import gfx.draw
from highvale.land.palette import SHADE
from highvale.view.eye import MID_X

' the characters, and each one's glyph: 7 rows of 5, 1 lit
const BIG_CHARACTERS = " !0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"
const BIG_GLYPHS = "000000000000000000000000000000000000010000100" + _
    "001000010000100000000010001110100011001110101" + _
    "110011000101110001000110000100001000010000100" + _
    "011100111010001000010001000100010001111111110" + _
    "000010000101110000010000111110000100011001010" + _
    "100101111100010000101111110000111100000100001" + _
    "100010111001110100001000011110100011000101110" + _
    "111110000100010001000100001000010000111010001" + _
    "100010111010001100010111001110100011000101111" + _
    "000010000101110011101000110001111111000110001" + _
    "100011111010001100011111010001100011111001110" + _
    "100011000010000100001000101110111101000110001" + _
    "100011000110001111101111110000100001111010000" + _
    "100001111111111100001000011110100001000010000" + _
    "011101000110000101111000110001011111000110001" + _
    "100011111110001100011000101110001000010000100" + _
    "001000010001110001110001000010000100001010010" + _
    "011001000110010101001100010100100101000110000" + _
    "100001000010000100001000011111100011101110101" + _
    "101011000110001100011000110001110011010110011" + _
    "100011000101110100011000110001100011000101110" + _
    "111101000110001111101000010000100000111010001" + _
    "100011000110101100100110111110100011000111110" + _
    "101001001010001011111000010000011100000100001" + _
    "111101111100100001000010000100001000010010001" + _
    "100011000110001100011000101110100011000110001" + _
    "100011000101010001001000110001100011010110101" + _
    "101010101010001100010101000100010101000110001" + _
    "100011000101010001000010000100001001111100001" + _
    "0001000100010001000011111"
const BIG_W = 5
const BIG_H = 7
' a glyph's cell, its width and a gap
const BIG_CELL = 6

' Row r, from 0, of character c's glyph as 1s and 0s;
' blank for one the font lacks.
function big_row (c as string, byval r as integer) as string
    dim n as integer

    n = instr(BIG_CHARACTERS, c)
    big_row = "00000"
    if n = 0 then exit function
    big_row = mid$(BIG_GLYPHS, (n - 1) * 35 + r * BIG_W + 1, BIG_W)
end function

' Text in big letters, each bit a square size wide,
' centred on x.
sub big_text ( _
    byval dst as long, _
    byval x as integer, _
    byval y as integer, _
    s as string, _
    byval size as integer, _
    byval colour as integer _
)
    dim i as integer
    dim r as integer
    dim b as integer
    dim left_x as integer
    dim gx as integer
    dim gy as integer
    dim bits as string

    left_x = x - (len(s) * BIG_CELL - 1) * size \ 2

    for i = 1 to len(s)
        for r = 0 to BIG_H - 1
            bits = big_row(mid$(s, i, 1), r)

            for b = 1 to BIG_W
                if mid$(bits, b, 1) = "0" then continue for
                gx = left_x + ((i - 1) * BIG_CELL + b - 1) * size
                gy = y + r * size
                gfx.draw.rect dst, gx, gy, gx + size - 1, _
                    gy + size - 1, colour, gfx.draw.FILL
            next
        next
    next
end sub

' A note in big letters with a shadow, centred in the
' view at row y.
sub draw_note ( _
    byval dst as long, _
    s as string, _
    byval y as integer, _
    byval size as integer, _
    byval colour as integer _
)
    big_text dst, MID_X + 1, y + 1, s, size, SHADE
    big_text dst, MID_X, y, s, size, colour
end sub
