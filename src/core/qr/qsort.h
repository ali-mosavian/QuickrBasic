/* a.sort's two sorts, over elements known by index only. How it works: qsort.c. */
#ifndef QR_QSORT_H
#define QR_QSORT_H
#include "base.h"

/* A sort's elements: compared and exchanged by index through these, its own data after them. */
typedef struct QSort QSort;
struct QSort {
    bool (*less)(QSort *s, uint32_t a, uint32_t b);
    void (*swap)(QSort *s, uint32_t a, uint32_t b);
};

/* The words qsort_merge's buffer takes for n elements. */
static inline size_t qsort_room(const uint32_t n) { return 3 * (size_t)n; }
/* Elements 0 .. n-1 in order, the equal kept in theirs: a natural merge sort, the elements in
   place while compared, then moved by swaps. buf: qsort_room(n) words. */
void qsort_merge(QSort *s, uint32_t n, uint32_t *buf);
/* Elements 0 .. n-1 in order, the equal in any: a balanced quicksort, no buffer. */
void qsort_quick(QSort *s, uint32_t n);
#endif
