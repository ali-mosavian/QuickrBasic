/* The debug pane's rows' text: pieces copied, numbers spelled, a row cut to the pane. */
#include "ui/debugtext.h"

char *dtxt_cpy(char *const d, const char *const s)
{
    uint32_t i = 0;

    for (; s[i]; i++)
        d[i] = s[i];

    return d + i;
}

char *dtxt_dec(char *d, const int32_t v)
{
    uint32_t u = (uint32_t)v;

    if (v < 0) {
        *d++ = '-';
        u = 0u - u;
    }

    char t[10];
    int n = 0;

    do {
        t[n++] = (char)('0' + u % 10);
        u /= 10;
    } while (u);

    for (int i = 0; i < n; i++)
        d[i] = t[n - 1 - i];

    return d + n;
}

void dtxt_cut(char *const d, const char *const s)
{
    uint32_t i = 0;

    for (; i < DP_TWMAX - 1 && s[i]; i++)
        d[i] = s[i];

    d[i] = 0;
}
