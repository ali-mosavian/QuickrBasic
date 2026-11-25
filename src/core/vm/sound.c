/* The speaker's services (syscalls.def): SOUND, PLAY, PLAY of voices and PLAY(n), into llrm's music queue
 * (qbrt/qbrt.h), which the runtime's tick plays; instrument, music, note and beat(), its songs.
 *
 * # asm structure
 * | asm | here | |
 * |---|---|---|
 * | (none) | snd_* | NEW |
 */
#include "session.h"
#include "io/repl.h"
#include "mem/array.h"
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
    const uint32_t given = (uint32_t)exec_popi(&S->vm);
    QbrtStr strings[QBRT_VOICES];
    QbrtStr *commands[QBRT_VOICES];
    const int32_t count = 32 - __builtin_clz(given);

    for (int32_t i = count - 1; i >= 0; i--) {
        commands[i] = nullptr;
        if (given >> i & 1) {
            const Desc d = strg_get(&S->strg, exec_popa(&S->vm));
            strings[i] = (QbrtStr){.len = d.len, .ptr = (char *)at(d.ptr)};
            commands[i] = &strings[i];
        }
    }
    B_SPLYV(commands, (unsigned)count);
}

/* ON PLAY looks too, at the next stop check: its own look is on the clock's tick, not the music's,
   and the program must not act on a fall it has not seen. */
void snd_waiting(Session *const S)
{
    (void)exec_popi(&S->vm);
    qbrt_tick_poll();
    S->vm.stop |= SF_TICK;
    exec_pushi(&S->vm, (int32_t)music_waiting());
}

void snd_instrument(Session *const S)
{
    const Desc d = strg_get(&S->strg, exec_popa(&S->vm));
    QbrtStr commands = {.len = d.len, .ptr = (char *)at(d.ptr)};

    song_instrument(exec_popi(&S->vm), &commands);
}

/* The events' record: a TYPE whose first five fields are INTEGERs or LONGs, 4 bytes each, one after another. */
static bool event_record(const Session *const S, const uint32_t et)
{
    enum { EVENT_FIELDS = 5, INTEGER_BYTES = 4 };

    if (et < ET_REC)
        return false;
    const TypeDef *const t = decl_type(&S->decl, et - ET_REC);

    if (t->nfld < EVENT_FIELDS)
        return false;
    for (uint32_t k = 0; k < EVENT_FIELDS; k++) {
        const Field *const f = &t->fields[k];

        if ((f->et != ET_I2 && f->et != ET_I4) || f->sub || f->ndim || f->dyn || f->off != k * INTEGER_BYTES)
            return false;
    }
    return true;
}

void snd_music(Session *const S)
{
    const int32_t from = exec_popi(&S->vm);
    (void)exec_popi(&S->vm); /* the element's kind */
    const Addr el = exec_popa(&S->vm);
    ArrView v;
    const ArrSlot *const s = exec_arrayop(&S->arry, &S->vm, &v);

    if (s && !s->hdr)
        repl_error(S, E_IQ);
    if (!event_record(S, v.et))
        repl_error(S, E_TM);
    const Addr end = v.data + arry_view_size(&S->arry, &v);
    const Addr first = el ? el : v.data;
    const uint32_t size = arry_et_size(&S->decl, v.et);

    song_play(at(first), (end - first) / size, size, from);
}

void snd_music_stop(Session *const S)
{
    song_stop(exec_popi(&S->vm));
}

void snd_note(Session *const S)
{
    const int32_t loud = exec_popi(&S->vm), length = exec_popi(&S->vm), pitch = exec_popi(&S->vm);

    song_note(exec_popi(&S->vm), pitch, length, loud);
}

void snd_beat(Session *const S)
{
    qbrt_tick_poll();
    exec_push(&S->vm, (Cell){.f = song_beat()});
}
