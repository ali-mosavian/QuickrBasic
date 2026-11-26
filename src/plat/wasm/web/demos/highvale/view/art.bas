' The pictures, drawn by rule at the start: the foes'
' sprites from rows of letters, a letter a flat tone, and
' the panel's font of 3 by 5 glyphs.
option explicit

import gfx.surface, gfx.draw
from highvale.land.palette import METAL, FOE_DARK, FOE_RED
from highvale.land.palette import BUNKER_DARK, BUNKER, AMBER
from highvale.land.palette import SMOKE, SMOKE_DARK, FLAME

' a sprite's letters and the tone each stands for
const LEGEND = "kdrbtasmo"
' the font's characters, and each one's glyph, 5 rows of
' 3 a glyph, 1 drawn
const CHARACTERS = " !-./0123456789:ABCDEFGHIJKLMNOPQRSTUVWXYZ"
const GLYPHS = "000000000000000010010010000010000000111000000" + _
    "000000000000010001001010100100111101101101111" + _
    "010110010010111111001111100111111001011001111" + _
    "101101111001001111100111001111111100111101111" + _
    "111001010010010111101111101111111101111001111" + _
    "000010000010000010101111101101110101110101110" + _
    "111100100100111110101101101110111100110100111" + _
    "111100110100100111100101101111101101111101101" + _
    "111010010010111001001001101111101101110101101" + _
    "100100100100111101111111101101110101101101101" + _
    "111101101101111111101111100100111101101111001" + _
    "110101110101101111100111001111111010010010010" + _
    "101101101101111101101101101010101101111111101" + _
    "101101010101101101101010010010111001010100111"
const GLYPH_W = 3
const GLYPH_H = 5
' a font cell, and the cells a row of the font's sheet
const CELL_W = 4
const CELL_H = 6
const SHEET_COLUMNS = 16

type Art
    sheet as long
    flak as long
    wreck as long
    drone(1) as long
    font as long
end type

' The tone a sprite's letter stands for; 0 none.
function tone_for (letter as string) as integer
    select case instr(LEGEND, letter)
        case 1
            tone_for = METAL
        case 2
            tone_for = FOE_DARK
        case 3
            tone_for = FOE_RED
        case 4
            tone_for = BUNKER_DARK
        case 5
            tone_for = BUNKER
        case 6
            tone_for = AMBER
        case 7
            tone_for = SMOKE
        case 8
            tone_for = SMOKE_DARK
        case 9
            tone_for = FLAME
        case else
            tone_for = 0
    end select
end function

' Rows of letters, split by bars, painted at x, y on s.
sub lay_rows ( _
    byval s as long, _
    byval x as integer, _
    byval y as integer, _
    rows as string _
)
    dim i as integer
    dim col as integer
    dim row as integer
    dim tone as integer
    dim letter as string

    for i = 1 to len(rows)
        letter = mid$(rows, i, 1)

        if letter = "|" then
            row = row + 1
            col = 0
            continue for
        end if

        tone = tone_for(letter)
        if tone then gfx.draw.pset s, x + col, y + row, tone
        col = col + 1
    next
end sub

' A view of the sheet with its anchor.
function cut ( _
    byval sheet as long, _
    byval x as integer, _
    byval w as integer, _
    byval h as integer, _
    byval ax as integer, _
    byval ay as integer _
) as long
    dim v as long

    v = gfx.surface.new_view(sheet, x, 0, w, h)
    gfx.surface.set_anchor v, ax, ay
    cut = v
end function

' The foes' sprites on one sheet, each a view of it.
sub make_sprites (a as Art)
    a.sheet = gfx.surface.new(64, 8)
    lay_rows a.sheet, 0, 0, "........kk..|.......kk...|" + _
        "......kk....|....bbbb....|...btttab...|" + _
        "..bttttttb..|.bttttttttb.|bbbbbbbbbbbb"
    lay_rows a.sheet, 16, 0, "....m..s....|..s.m.mm....|" + _
        "...bmbbm.s..|..bbtbbtb...|.bbbbbbbbbb."
    lay_rows a.sheet, 32, 0, "..kkkkkkkkk..|......k......|" + _
        "...ddddddd...|.drrrrrrrrrd.|ddrrrarrrrrrd|" + _
        ".drrrrrrrrrd.|...dd...dd..."
    lay_rows a.sheet, 48, 0, "....kkkkk....|......k......|" + _
        "...ddddddd...|.drrrrrrrrrd.|ddrrrrrrarrrd|" + _
        ".drrrrrrrrrd.|...dd...dd..."
    a.flak = cut(a.sheet, 0, 12, 8, 6, 7)
    a.wreck = cut(a.sheet, 16, 12, 5, 6, 4)
    a.drone(0) = cut(a.sheet, 32, 13, 7, 6, 3)
    a.drone(1) = cut(a.sheet, 48, 13, 7, 6, 3)
end sub

' Row r, from 0, of character c's glyph: GLYPH_W bits as
' 1s and 0s; blank for one the font lacks.
function glyph_row (c as string, byval r as integer) as string
    dim n as integer

    n = instr(CHARACTERS, c)
    glyph_row = "000"
    if n = 0 then exit function
    glyph_row = mid$(GLYPHS, (n - 1) * 15 + r * GLYPH_W + 1, GLYPH_W)
end function

' The panel's font: each glyph at its character's cell,
' from character 32, its lit bits colour 1.
sub make_font (a as Art)
    dim sheet as long
    dim i as integer
    dim r as integer
    dim b as integer
    dim code as integer
    dim x as integer
    dim y as integer
    dim bits as string

    sheet = gfx.surface.new(SHEET_COLUMNS * CELL_W, 4 * CELL_H)

    for i = 1 to len(CHARACTERS)
        code = asc(mid$(CHARACTERS, i, 1)) - 32
        x = (code mod SHEET_COLUMNS) * CELL_W
        y = (code \ SHEET_COLUMNS) * CELL_H

        for r = 0 to GLYPH_H - 1
            bits = glyph_row(mid$(CHARACTERS, i, 1), r)

            for b = 1 to GLYPH_W
                if mid$(bits, b, 1) = "0" then continue for
                gfx.draw.pset sheet, x + b - 1, y + r, 1
            next
        next
    next

    a.font = gfx.draw.new_font(sheet, CELL_W, CELL_H)
end sub

sub make_art (a as Art)
    make_sprites a
    make_font a
end sub
