/* Names as the program spells them. How it works: spelling.c. */
#ifndef UI_SPELLING_H
#define UI_SPELLING_H
#include "ui/ui.h"

/* What a name is spelled as: a variable of the procedure read, of its module, a TYPE's field. */
typedef enum : uint8_t { SPEL_LOCAL, SPEL_MODULE, SPEL_FIELD } SpelKind;

/* The spellings of view view's names, then of its file's module lines'. */
void spel_read(Session *S, uint32_t view);
/* Name name's spelling as a kind, as the first line read that spells it so does, its suffix left
   out, at d; none, the name's own (capitals). A local none: the module's. Past it. */
char *spel_name(const Session *S, char *d, uint32_t name, SpelKind kind);
#endif
