/* Native code: not yet loaded on this build (docs/native-extensions-design.md, step 2). No tag,
   so a .qbx's run stops at its IMPORT: Native code not built for this machine. */
#include "plat/pext.h"

const char pext_tag[] = "";

QxInit *pext_load(const void *const slice, const uint32_t bytes, const char *const name)
{
    (void)slice;
    (void)bytes;
    (void)name;
    return nullptr;
}
