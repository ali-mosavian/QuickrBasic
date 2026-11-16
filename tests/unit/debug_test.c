/* The session's debugger: what a step asks the VM's next DBG op for. */
#include "session.h"
#include "io/repl.h"
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

TEST(frames_list_the_stop_then_each_callers_line)
{
    S->vm.cont.line = 30;
    S->vm.frames[0] = (Frame){.kind = FRAME_CALL, .ret.line = 10, .call.proc = 1};

    S->vm.frames[1] = (Frame){.kind = FRAME_GOSUB, .ret.line = 20};

    S->vm.frames[2] = (Frame){.kind = FRAME_CALL, .ret.line = 25, .call.proc = 2};

    S->vm.nframes = 3;
    DebugFrame f[4];
    CHECK(session_frames(S, f, 4) == 3);
    CHECK(f[0].line == 30 && f[0].proc == 2 && f[0].call == 2);
    CHECK(f[1].line == 25 && f[1].proc == 1 && f[1].call == 0);
    CHECK(f[2].line == 10 && f[2].proc == 0 && f[2].call == -1);
    CHECK(session_frames(S, f, 2) == 2);
    session_drop_stop(S);
}

/* Each call's by-value parameter as its frame sees it: each call's is its own value cell. */
TEST(an_older_calls_parameter_by_value_is_its_own)
{
    static const char *const lines[] = {
        "10 r 3",
        "20 SUB r (n%)",
        "30 IF n% > 1 THEN r n% - 1 ELSE STOP",
        "40 END SUB",
        nullptr,
    };

    unit_run(S, lines);
    DebugFrame f[4];
    CHECK(session_frames(S, f, 4) == 4);
    const uint16_t n = blok_proc(&S->blok, f[0].proc)->params[0].var;

    for (int32_t i = 0; i < 3; i++)
        CHECK((int16_t)rd32(session_cell(S, &f[i], n, 0)) == i + 1);

    session_drop_stop(S);
}

/* A stop left the data stack at the module's base, above the calls' frames: a watch's "1 / 0"
   folded 1.0 over the outer call's argument, its N% then 1065353216. */
TEST(a_probe_leaves_an_older_calls_argument)
{
    static const char *const lines[] = {
        "10 r 4",
        "20 SUB r (n%)",
        "30 IF n% > 3 THEN r n% - 1 ELSE STOP",
        "40 END SUB",
        nullptr,
    };

    unit_run(S, lines);
    DebugFrame f[3];
    CHECK(session_frames(S, f, 3) == 3);
    const uint16_t n = blok_proc(&S->blok, f[0].proc)->params[0].var;
    char out[80];
    CHECK(dbug_eval(S, "1 / 0", f[0].proc, out) != ST_OK);
    CHECK((int16_t)rd32(session_cell(S, &f[1], n, 0)) == 4);
    session_drop_stop(S);
}

/* CONT after a stop ran on the module's stack base: the inner call's PRINT n% pushed 3 over the
   outer call's argument, which then printed 3, not 4. */
TEST(cont_leaves_an_older_calls_argument)
{
    static const char *const lines[] = {
        "10 r 4",
        "20 SUB r (n%)",
        "30 IF n% > 3 THEN r n% - 1 ELSE STOP",
        "40 PRINT n%;",
        "50 END SUB",
        nullptr,
    };

    unit_run(S, lines);
    S->cons.capture = CAP_SBUF;
    S->cons.sblen = 0;
    repl_call(S, repl_cont);
    S->cons.capture = 0;
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, " 3  4 ");
}
