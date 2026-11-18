# GET and PUT Statements (graphics)
keys: GET (graphics), PUT (graphics), GET graphics, PUT graphics

Syntax
    GET [STEP](x1, y1)-[STEP](x2, y2), array
    PUT [STEP](x, y), array[, action]

GET copies a screen rectangle into a numeric array; PUT draws it
back with its top-left corner at (x,y).

See also [GET and PUT (files)](GET), [SCREEN modes](SCREEN modes),
[LINE](LINE), [PAINT](PAINT)

## Details
GET copies the rectangle with corners (x1,y1) and (x2,y2) from the
screen into a numeric array. PUT draws it back with its top-left
corner at (x,y). Coordinates follow the current WINDOW or VIEW. A
sprite that falls partly outside the screen is an Illegal function
call, except with the action PSET or XOR.

Action is how the image meets what is already there:

    PSET   replace    PRESET  replace with inverse colours
    AND    keep only colours both share
    OR     overlay            XOR (default) draw, and a second PUT
                              of the same image erases it again

The array must be big enough: for the image in a SCREEN mode with
b bits per pixel and w by h pixels, it takes about
4 + h * INT((w * b + 7) / 8) bytes, in an INTEGER array of half as
many elements. Size it generously. The image keeps the screen mode's
format, so it cannot be used in another mode.

## Example
    SCREEN 13
    CIRCLE (10, 10), 8, 4
    DIM ball(100) AS INTEGER
    GET (0, 0)-(20, 20), ball
    PUT (150, 90), ball, PSET     ' a copy of the circle at (150,90)
