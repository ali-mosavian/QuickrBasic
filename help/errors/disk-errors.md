# Device I/O error / Disk full / Disk not ready / Disk-media error
keys: ~Device I/O error, Disk full, Disk not ready, Disk-media error, error 57, error 61, error 71, error 72

Errors 57, 61, 71 and 72

The disk could not be read or written.

See also: [OPEN](OPEN), [ON ERROR](ON ERROR)

## Details

Cause: The disk could not be read or written: it is full, the drive
door is open or empty, or the disk is damaged.

Remedy: Free space, insert a good disk and close the drive, then try
again. Trap the error with [ON ERROR](ON ERROR) to let the user retry.

Error 57 Device I/O error, 61 Disk full, 71 Disk not ready, 72
Disk-media error.

Trap it with ON ERROR; ERR returns the number.

## Example
    ON ERROR GOTO h
    OPEN "A:X.TXT" FOR OUTPUT AS #1   ' no disk: error 71
    END
    h:
    PRINT "disk error"; ERR
    RESUME NEXT
