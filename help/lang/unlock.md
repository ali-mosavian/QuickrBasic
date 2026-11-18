# UNLOCK Statement
keys: UNLOCK

Syntax
    UNLOCK [#]filenumber [, record | [start] TO end]

Releases a lock set with LOCK.

See also [LOCK](LOCK), [OPEN](OPEN), [files](files)

## Details

Releases a lock set with LOCK. The range must match the LOCK's. With
no range the whole file is unlocked.

UNLOCK every lock before CLOSE. A range that was never locked is a
"Permission denied" error.

## Example
    OPEN "T.DAT" FOR BINARY AS #1
    LOCK #1, 5
    UNLOCK #1, 5
    CLOSE #1
