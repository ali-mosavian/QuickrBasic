# keys Module
keys: ~quickr keys, ~quickr keys.Key, ~quickr keys.held_for, ~quickr keys.next, ~quickr keys.pressed, ~quickr keys.down, ~quickr keys.Mouse, ~quickr keys.mouse, ~quickr keys.clicked

The keyboard, as INKEY$ reads it, and the mouse, as PEN does.

    next() as int                  the next key's code, 0 none waiting
    pressed(k as Key) as bool      k pressed since the last pressed(k)
    down(k as Key) as bool         k pressed or repeated lately
    k.code() as int                k's code, as next() gives it
    mouse() as Mouse               its pixel x, y; left, right down
    clicked(right = false) as bool a button pressed since the last look

See also [Quickr](quickr index), [time](quickr time)

## Details
Key is escape, enter, space, tab, backspace, up, down, left, right
and a to z. The platforms report presses and repeats, never a
release: down(k) holds for held_for seconds (0.55) after k's last.

## Example
    option dialect quickr
    import keys
    while not keys.pressed(keys.Key.escape)
        if keys.down(keys.Key.left) then print "left"
    wend
