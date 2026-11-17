/* The speaker's services (syscalls.def): SOUND, PLAY, PLAY of voices and PLAY(n), into llrm's music queue
 * (qbrt/qbrt.h), which the runtime's tick plays.
 *
 * # asm structure
 * | asm | here | |
 * |---|---|---|
 * | (none) | snd_* | NEW |
 */
#include "session.h"
#include "mem/string.h"
#include "qbrt/qbrt.h"

void snd_sound(Session *const S)
{
    const float ticks = (float)exec_popd(&S->vm);
    B_SOND((short)exec_popi(&S->vm), ticks);
}

void snd_play(Session *const S)
{
    const Desc d = strg_get(&S->strg, exec_popa(&S->vm));
    QbrtStr commands = {.len = d.len, .ptr = (char *)at(d.ptr)};

    B_SPLY(&commands);
}

void snd_play_voices(Session *const S)
{
    const int32_t count = exec_popi(&S->vm);
    QbrtStr strings[QBRT_VOICES];
    QbrtStr *commands[QBRT_VOICES];

    for (int32_t i = count - 1; i >= 0; i--) {
        const Desc d = strg_get(&S->strg, exec_popa(&S->vm));
        strings[i] = (QbrtStr){.len = d.len, .ptr = (char *)at(d.ptr)};
        commands[i] = &strings[i];
    }
    B_SPLYV(commands, (unsigned)count);
}

void snd_waiting(Session *const S)
{
    (void)exec_popi(&S->vm);
    exec_pushi(&S->vm, (int32_t)music_waiting());
}
