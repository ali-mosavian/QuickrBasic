# Keyboard Scan Codes
keys: scan codes, scan code, keyboard codes, extended keys, key codes

Two kinds of code identify a key press.

See also [ASCII](ASCII), [INKEY$](INKEY$), [KEY](KEY)

## Details
INKEY$ and extended keys
[INKEY$](INKEY$) returns one character for most keys. For keys with no
character, such as the arrows and function keys, it returns two:
CHR$(0) and the code below.

```
Key        Code    Key        Code    Key          Code
F1          59     Home        71     Alt+A         30
F2          60     Up          72     Alt+B         48
F3          61     PgUp        73     Alt+C         46
F4          62     Left        75     Alt+X         45
F5          63     Right       77     Shift+Tab     15
F6          64     End         79     Ctrl+Left    115
F7          65     Down        80     Ctrl+Right   116
F8          66     PgDn        81     Ctrl+End     117
F9          67     Ins         82     Ctrl+PgDn    118
F10         68     Del         83     Ctrl+Home    119
F11        133     Alt+1      120     Ctrl+PgUp    132
F12        134     Alt+0      129
```

Scan codes for KEY and ON KEY
[KEY](KEY) n, CHR$(shift) + CHR$(scan) defines a user key trap. n is
15 to 25; the string holds a shift mask and the scan code of the key:
```
mask  adds           mask  adds
 &H01  Right Shift    &H20  Num Lock
 &H02  Left Shift     &H40  Caps Lock
 &H04  Ctrl           &H80  an extended key
 &H08  Alt
```

Scan codes of the main keys
```
Esc   1      1-0   2-11     -  12     =  13   Bksp 14   Tab  15
Q-P  16-25   [ 26  ] 27    Enter 28   Ctrl 29
A-L  30-38   ; 39  ' 40    ` 41   L-Shift 42   \ 43
Z-M  44-50   , 51  . 52    / 53   R-Shift 54   * 55
Alt  56      Space 57      CapsLk 58   F1-F10 59-68
NumLk 69     ScrLk 70      Home 71   Up 72   PgUp 73   - 74
Left 75      5 76          Right 77  + 78   End 79   Down 80
PgDn 81      Ins 82        Del 83
```
The cursor-pad scan codes are shared with the number pad.

## Example
    DO: k$ = INKEY$: LOOP WHILE k$ = ""
    IF LEN(k$) = 2 THEN PRINT ASC(RIGHT$(k$, 1))
    ' press F1: 59
