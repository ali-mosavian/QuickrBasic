/* a.sort's two sorts, over elements known by index only.
 *
 * # How it works
 * With a lambda, qsort_merge: stable. The lambda reads the elements where they are, so they stay
 * put while compared; the sort orders their indexes. The ascending runs are found first, n - 1
 * compares, then merged pairwise, a pass at a time, between two halves of the buffer: sorted
 * input takes the n - 1 alone. The order found is applied last by swaps along its cycles, n - 1
 * at most.
 *
 * Without one, qsort_quick: numbers and strings, whose equal values can't be told apart. Its
 * pivot is the median of three, or of three medians of three (Tukey's ninther) past
 * QSORT_NINTHER, so sorted and reversed input split evenly. The partition is three-way, the keys
 * equal to the pivot left out of both sides, so many equal keys don't go quadratic. The smaller
 * side recurses and the larger loops: the depth is O(log n). Short ranges are insertion sorted.
 */
#include "qr/qsort.h"

enum { QSORT_SMALL = 12, QSORT_NINTHER = 40 };

static void qsort_exch(QSort *const s, const uint32_t a, const uint32_t b)
{
    if (a != b)
        s->swap(s, a, b);
}

/* ---- the merge sort */

/* Runs src[lo, mid) and src[mid, hi) merged to dst, the left's first when equal. */
static void qsort_join(
    QSort *const s,
    const uint32_t *const src,
    uint32_t *const dst,
    const uint32_t lo,
    const uint32_t mid,
    const uint32_t hi
)
{
    uint32_t a = lo, b = mid, k = lo;

    while (a < mid && b < hi)
        dst[k++] = s->less(s, src[b], src[a]) ? src[b++] : src[a++];

    memcpy(dst + k, src + a, (mid - a) * sizeof *dst);
    k += mid - a;
    memcpy(dst + k, src + b, (hi - b) * sizeof *dst);
}

/* Element order[k] moved to k for each k, along the cycles; order made the identity. */
static void qsort_apply(QSort *const s, uint32_t *const order, const uint32_t n)
{
    for (uint32_t k = 0; k < n; k++) {
        uint32_t j = k;

        while (order[j] != k) {
            const uint32_t next = order[j];
            s->swap(s, j, next);
            order[j] = j;
            j = next;
        }

        order[j] = j;
    }
}

void qsort_merge(QSort *const s, const uint32_t n, uint32_t *const buf)
{
    if (n < 2)
        return;

    uint32_t *src = buf, *dst = buf + n;
    uint32_t *const ends = buf + 2 * n;
    uint32_t runs = 0;

    for (uint32_t k = 0; k < n; k++)
        src[k] = k;

    for (uint32_t k = 1; k <= n; k++)
        if (k == n || s->less(s, k, k - 1))
            ends[runs++] = k;

    while (runs > 1) {
        uint32_t lo = 0, m = 0;

        for (uint32_t r = 0; r < runs; r += 2) {
            const uint32_t mid = ends[r];
            const uint32_t hi = r + 1 < runs ? ends[r + 1] : mid;
            qsort_join(s, src, dst, lo, mid, hi);
            ends[m++] = hi;
            lo = hi;
        }

        runs = m;
        uint32_t *const t = src;
        src = dst;
        dst = t;
    }

    qsort_apply(s, src, n);
}

/* ---- the quicksort */

static uint32_t qsort_median(QSort *const s, uint32_t a, uint32_t b, const uint32_t c)
{
    if (s->less(s, b, a)) {
        const uint32_t t = a;
        a = b;
        b = t;
    }

    if (!s->less(s, c, b))
        return b;

    return s->less(s, c, a) ? a : c;
}

static uint32_t qsort_pivot(QSort *const s, const uint32_t lo, const uint32_t hi)
{
    const uint32_t n = hi - lo, mid = lo + n / 2, last = hi - 1;

    if (n < QSORT_NINTHER)
        return qsort_median(s, lo, mid, last);

    const uint32_t e = n / 8;
    return qsort_median(
        s,
        qsort_median(s, lo, lo + e, lo + 2 * e),
        qsort_median(s, mid - e, mid, mid + e),
        qsort_median(s, last - 2 * e, last - e, last)
    );
}

static void qsort_insert(QSort *const s, const uint32_t lo, const uint32_t hi)
{
    for (uint32_t i = lo + 1; i < hi; i++)
        for (uint32_t j = i; j > lo && s->less(s, j, j - 1); j--)
            s->swap(s, j, j - 1);
}

/* [lo, hi) sorted. The pivot's equal stay at [lt, i) as the partition goes: < before, > past gt. */
static void qsort_range(QSort *const s, uint32_t lo, uint32_t hi)
{
    while (hi - lo > QSORT_SMALL) {
        qsort_exch(s, lo, qsort_pivot(s, lo, hi));
        uint32_t lt = lo, i = lo + 1, gt = hi;

        while (i < gt)
            if (s->less(s, i, lt))
                s->swap(s, lt++, i++);
            else if (s->less(s, lt, i))
                qsort_exch(s, i, --gt);
            else
                i++;

        if (lt - lo < hi - gt) {
            qsort_range(s, lo, lt);
            lo = gt;
        } else {
            qsort_range(s, gt, hi);
            hi = lt;
        }
    }

    qsort_insert(s, lo, hi);
}

void qsort_quick(QSort *const s, const uint32_t n) { qsort_range(s, 0, n); }
