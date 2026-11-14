/* The session's debugger: what a step asks the VM's next DBG op for. */
#include "session.h"
#include "unit.h"

TEST(step_over_and_out_stop_at_the_stopped_depth)
{
    S->vm.frames[0].kind = FRAME_CALL;
    S->vm.frames[1].kind = FRAME_GOSUB;
    S->vm.nframes = 2;
    session_step(S, STEP_OVER, true);
    CHECK(S->dbug.mode == DM_PSTEP && S->dbug.pdep == 2);
    session_step(S, STEP_OUT, true);
    CHECK(S->dbug.mode == DM_PSTEP && S->dbug.pdep == 1);
    session_step(S, STEP_OUT, false);
    CHECK(S->dbug.pdep == 0);
    session_step(S, STEP_INTO, true);
    CHECK(S->dbug.mode == DM_STEP);
    session_drop_stop(S);
}
