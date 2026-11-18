# ASCII Characters
keys: ASCII, ASCII table, character codes, CHR$ codes, code page 437

Each character has a code from 0 to 255, given by [ASC](ASC) and
turned into a character by [CHR$](CHR$). QuickrBasic uses the IBM PC
character set (code page 437), as QBasic did.

## 0 to 31: control codes
They have a name. [PRINT](PRINT)
acts on 7, 8, 9, 10, 12 and 13 instead of drawing them.

```
 0 NUL    1 SOH    2 STX    3 ETX
 4 EOT    5 ENQ    6 ACK    7 BEL
 8 BS     9 HT    10 LF    11 VT
12 FF    13 CR    14 SO    15 SI
16 DLE   17 DC1   18 DC2   19 DC3
20 DC4   21 NAK   22 SYN   23 ETB
24 CAN   25 EM    26 SUB   27 ESC
28 FS    29 GS    30 RS    31 US
```
7 beeps, 8 backspaces, 9 tabs, 10 ends a line, 12 clears the screen,
13 returns the cursor to the line start. CHR$(0) before a code marks
an extended key in [INKEY$](INKEY$); see [Scan codes](scan codes).

## 32 to 127: text
```
 32      44 ,    56 8    68 D    80 P    92 \   104 h   116 t
 33 !    45 -    57 9    69 E    81 Q    93 ]   105 i   117 u
 34 "    46 .    58 :    70 F    82 R    94 ^   106 j   118 v
 35 #    47 /    59 ;    71 G    83 S    95 _   107 k   119 w
 36 $    48 0    60 <    72 H    84 T    96 `   108 l   120 x
 37 %    49 1    61 =    73 I    85 U    97 a   109 m   121 y
 38 &    50 2    62 >    74 J    86 V    98 b   110 n   122 z
 39 '    51 3    63 ?    75 K    87 W    99 c   111 o   123 {
 40 (    52 4    64 @    76 L    88 X   100 d   112 p   124 |
 41 )    53 5    65 A    77 M    89 Y   101 e   113 q   125 }
 42 *    54 6    66 B    78 N    90 Z   102 f   114 r   126 ~
 43 +    55 7    67 C    79 O    91 [   103 g   115 s   127 DEL
```
Code 32 is the space. Code 127 is DEL.

## 128 to 255
Accents, box drawing, shading blocks and math signs. The row is the
first hex digit of the code, the column the second: the character in
row B, column 3 is code 179 (hex B3), a vertical line.

```
     0 1 2 3 4 5 6 7 8 9 A B C D E F
8_   Ç ü é â ä à å ç ê ë è ï î ì Ä Å
9_   É æ Æ ô ö ò û ù ÿ Ö Ü ¢ £ ¥ ₧ ƒ
A_   á í ó ú ñ Ñ ª º ¿ ⌐ ¬ ½ ¼ ¡ « »
B_   ░ ▒ ▓ │ ┤ ╡ ╢ ╖ ╕ ╣ ║ ╗ ╝ ╜ ╛ ┐
C_   └ ┴ ┬ ├ ─ ┼ ╞ ╟ ╚ ╔ ╩ ╦ ╠ ═ ╬ ╧
D_   ╨ ╤ ╥ ╙ ╘ ╒ ╓ ╫ ╪ ┘ ┌ █ ▄ ▌ ▐ ▀
E_   α ß Γ π Σ σ µ τ Φ Θ Ω δ ∞ φ ε ∩
F_   ≡ ± ≥ ≤ ⌠ ⌡ ÷ ≈ ° ∙ · √ ⁿ ² ■
```
Codes 176 to 178 are light, medium and dark shade; 219 is a full block;
179 to 218 draw single and double lines.

See also [Scan codes](scan codes), [Color attributes](color attributes), [CHR$](CHR$)
