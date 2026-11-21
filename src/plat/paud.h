/* The host's audio stream: each render opened, mixed into what the device plays.
 *
 * # How it works
 * A platform's device (the Sound Blaster, SDL's stream) makes itself known with paud_device. The
 * first stream opened starts it, the last closed stops it. The device calls paud_mix where it
 * plays, an interrupt or an audio thread: each render's frames, summed and clipped. Code that
 * changes what a render reads holds the device off with paud_lock. plat/audio/paud.c is the
 * portable one; wasm's are plat/wasm's.
 */
#ifndef PLAT_PAUD_H
#define PLAT_PAUD_H
#include "qx/qx.h" /* C11 alone: the runtime's devices include it */

/* A render: frames into out, at the device's rate, a frame its left sample then its right when
   it is stereo. Run where the device plays. */
typedef void QX_CALL PaudRender(int16_t *out, int32_t frames);

/* A device: what it plays; started by the first stream (false: it cannot), stopped by the last;
   its lock, which holds its playing off, its token for its unlock. */
typedef struct {
    uint32_t rate;
    bool stereo;
    bool (*start)(void);
    void (*stop)(void);
    uint32_t (*lock)(void);
    void (*unlock)(uint32_t token);
} PaudDevice;

/* The platform's device, the process's; null none. */
void paud_device(const PaudDevice *d);
/* Render r played: the device's rate and channels to *rate, *stereo. false: no device, or it
   would not start. Open already: as it was. */
bool paud_open(PaudRender *r, uint32_t *rate, bool *stereo);
/* Render r no longer played; the device stopped with the last. */
void paud_close(PaudRender *r);
/* Every render closed: the process ends. */
void paud_end(void);
/* The device's playing held off till paud_unlock with the token returned; nests. */
uint32_t paud_lock(void);
void paud_unlock(uint32_t token);
/* The device's: frames of each render summed into out, silence with none. */
void paud_mix(int16_t *out, int32_t frames);
#endif
