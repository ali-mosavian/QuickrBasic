/* What clang's code asks of a C library, in a driver, which imports nothing: _fltused, which a
 * function using floating point refers to. Copies and fills are the runtime's mem.asm.
 */
int _fltused;
