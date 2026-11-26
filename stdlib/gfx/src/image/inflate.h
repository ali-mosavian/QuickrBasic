/* Inflate: a zlib stream (RFC 1950) of deflate's blocks (RFC 1951) to its bytes, for PNG.
 *
 * # How it works
 * Stored, fixed and dynamic blocks; a code is read a bit at a time through its counts of each
 * length, as zlib's puff does: small, and fast enough for the images a game loads. The bytes go
 * into the caller's buffer, its size known from the image; more is an error, as is a bad
 * Adler-32.
 */
#ifndef GFX_INFLATE_H
#define GFX_INFLATE_H
#include <stdint.h>

/* in's n bytes inflated into out's size: false, not a zlib stream, or more than size. *made, the
   bytes made. */
bool inflate_zlib(const uint8_t *in, uint32_t n, uint8_t *out, uint32_t size, uint32_t *made);
#endif
