/* No runtime on this target yet: its start keeps the fault, its entries Advanced feature
   unavailable. */
#include "qbrt/qbrt.h"

static Fault *fault;

void qbrt_init(Fault *const f) { fault = f; }

[[noreturn]]
static void none(void)
{
    fault_qberr(fault, QB_NO_ADVANCED);
}

void screen_mode(const int *const block) { none(); }
void B_N1R4(const float x, const float y) { none(); }
void B_N2R4(const float x, const float y) { none(); }
void B_S1R4(const float x, const float y) { none(); }
void B_S2R4(const float x, const float y) { none(); }
void B_PSTC(const short color) { none(); }
void B_PSET(void) { none(); }
void B_PRST(void) { none(); }
void B_LINE(const short color, const short style, const short how) { none(); }
void B_CSTT(const float start) { none(); }
void B_CSTO(const float end) { none(); }
void B_CASP(const float aspect) { none(); }
void B_CIRC(const float radius, const short color) { none(); }
void B_PAIN(const short fill, const short border) { none(); }
void B_PNTC(QbrtStr *const tile, const short border, const short background) { none(); }
void B_DRAW(QbrtStr *const commands) { none(); }
void B_VIEW(
    const short x1,
    const short y1,
    const short x2,
    const short y2,
    const short fill,
    const short border,
    const short screen
)
{
    none();
}
void B_VEW0(void) { none(); }
void B_WIND(const float x1, const float y1, const float x2, const float y2, const short screen)
{
    none();
}
void B_WIN0(void) { none(); }
void B_PAL2(const short attribute, const long color) { none(); }
void qbrt_palette_using(void *const data, const uint32_t bytes, const uint32_t elem) { none(); }
void qbrt_get(void *const data, const uint32_t bytes, const uint32_t elem) { none(); }
void qbrt_put(void *const data, const uint32_t bytes, const uint32_t elem, const short how)
{
    none();
}
int B_PNR4(const float x, const float y) { none(); }
float *B_PMAP(const float value, const short function) { none(); }
