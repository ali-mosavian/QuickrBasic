/* The host's audio stream on wasm: none yet. The page's AudioWorklet is the device, a stream there
   a side module's render run on the shared memory (docs/native-extensions-design.md, step 3). */
#include "plat/paud.h"

bool paud_open(PaudRender *const r, uint32_t *const rate, bool *const stereo)
{
    (void)r;
    (void)rate;
    (void)stereo;
    return false;
}

void paud_end(void) {}

uint32_t paud_lock(void) { return 0; }

void paud_unlock(const uint32_t token) { (void)token; }
