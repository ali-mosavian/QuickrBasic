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
