/* docs/play-tutorial.md's examples: each ```basic block runs, and sounds. */
#include <stdio.h>
#include <stdlib.h>
#include "session.h"
#include "qbrt/qbrt.h"
#include "unit.h"

size_t headless_tone_count(void);
size_t headless_voice_count(void);
void headless_run(long hundredths);

enum { DOC_BYTES = 64 * 1024, DRAIN_HUNDREDTHS = 6000 };

TEST(play_tutorial_examples_run)
{
    static const char open_fence[] = "```basic\n", close_fence[] = "```\n";
    static char doc[DOC_BYTES];
    FILE *const f = fopen("docs/play-tutorial.md", "rb"); /* the tests run from the repository's top */
    CHECK(f);
    if (!f)
        return;
    const size_t n = fread(doc, 1, sizeof doc - 1, f);
    fclose(f);
    doc[n] = 0;

    qbrt_realtime(false);
    int examples = 0;
    for (const char *at = strstr(doc, open_fence); at; at = strstr(at, open_fence)) {
        at += sizeof open_fence - 1;
        const char *const end = strstr(at, close_fence);
        CHECK(end);
        if (!end)
            break;
        const size_t sounds = headless_tone_count() + headless_voice_count();
        const Status st = unit_run_bytes(S, at, (size_t)(end - at));
        if (st != ST_OK)
            fprintf(stderr, "example %d failed: %.*s\n", examples + 1, (int)(end - at), at);
        CHECK(st == ST_OK);
        headless_run(DRAIN_HUNDREDTHS); /* background music still queued */
        CHECK(headless_tone_count() + headless_voice_count() > sounds);
        music_clear();
        examples++;
        at = end;
    }
    qbrt_realtime(true);
    CHECK(examples >= 20);
}
