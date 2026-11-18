# DEF SEG Statement
keys: DEF SEG

Syntax
    DEF SEG [= segment]

DEF SEG sets the segment that [PEEK](PEEK), [POKE](POKE), BLOAD
and BSAVE count their offsets from. Segment is 0 to
65535. With no argument the segment goes back to the program's data
segment.

The setting stays until changed. Memory is addressed as segment * 16
+ offset, as on a DOS PC; on other platforms the same values reach
QuickrBasic's emulated PC memory (screen buffer at &HB800 for text,
&HA000 for graphics), not the host's.

## Example
    DEF SEG = &HB800
    POKE 0, ASC("A")     ' an A in the top-left cell
    DEF SEG

See also [PEEK](PEEK), [POKE](POKE), [VARSEG](VARSEG)
