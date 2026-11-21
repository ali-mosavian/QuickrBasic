/* The themes' attributes. */
#include "ui/theme.h"
#include "unit.h"

/* SL_OCC was QBasic's string colour: a use of a name looked like a string. Each mark on the text
   differs from each colour the text has, and from the other marks, in every theme. */
TEST(theme_marks_differ_from_the_text)
{
    static const uint8_t marks[] = {SL_ERR, SL_FIND, SL_MATCH, SL_OCC};

    static const uint8_t text[] = {
        SL_ED,
        SL_SEL,
        SL_KW,
        SL_STR,
        SL_NUM,
        SL_COM,
        SL_NAM,
        SL_FNM,
        SL_PAR,
        SL_TYP,
        SL_OP,
        SL_CUR
    };

    for (uint8_t t = 0; t < NTHEME; t++) {
        them_set(t);

        for (uint32_t m = 0; m < countof(marks); m++) {
            for (uint32_t s = 0; s < countof(text); s++)
                CHECK(them_attr(marks[m]) != them_attr(text[s]));
            for (uint32_t o = m + 1; o < countof(marks); o++)
                CHECK(them_attr(marks[m]) != them_attr(marks[o]));
        }
    }

    them_qbasic();
}
