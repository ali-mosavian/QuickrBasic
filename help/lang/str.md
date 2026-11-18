# STR$, VAL Functions
keys: STR$, VAL

Syntax
    STR$(numexpr)
    VAL(string$)

STR$ turns a number into its text, the same as PRINT shows it: a
blank in front of a number that is not negative, no blank after. Use
LTRIM$ to drop the blank.

VAL turns text into a number. It skips leading blanks, reads as many
characters as form a number, and stops at the first one that does
not. It understands &H and &O prefixes and D or E exponents. If
nothing numeric comes first, VAL returns 0. The result is DOUBLE.

## Example
    PRINT STR$(42); "|"; STR$(-7)       '  42|-7
    PRINT VAL("12abc"); VAL("&HFF")     '  12  255

See also [LTRIM$](LTRIM$), [CINT](CINT), [HEX$](HEX$), [MID$](MID$)
