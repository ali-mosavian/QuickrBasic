# Block IF / END IF / ELSE errors
keys: Block IF without END IF, END IF without block IF, ELSE without IF, error 224, error 225, error 226

Errors 224, 225 and 226

A block IF has no END IF, or an END IF or ELSE has no block IF.

See also: [IF](IF), [ELSE](ELSE), [END IF](END IF)

## Details

Cause: An [IF](IF) with nothing after THEN starts a block that needs
[END IF](END IF). Here the END IF is missing, or an END IF or
[ELSE](ELSE) has no block IF above it.

Remedy: Add the END IF after the block's last line. Or put the
statement after THEN on the same line and drop the END IF. Remove a
stray ELSE or END IF.

An IF with nothing after THEN starts a block. Error 224 Block IF
without END IF; 225 END IF without block IF; 226 ELSE without IF.

## Example
    IF x% > 0 THEN
      PRINT "positive"
    ' error 224: END IF missing
    IF x% > 0 THEN
      PRINT "positive"
    END IF               ' fixed
