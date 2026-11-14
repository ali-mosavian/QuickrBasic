#include "session.h"

void session_reset(Session *const S)
{
    S->scop.nbnd = 1;
    S->scop.depth = 1;
}
