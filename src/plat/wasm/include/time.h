#ifndef TIME_H
#define TIME_H
#include <sys/types.h>
struct timespec {
    time_t tv_sec;
    long tv_nsec;
};
struct tm {
    int tm_sec, tm_min, tm_hour, tm_mday, tm_mon, tm_year, tm_wday, tm_yday, tm_isdst;
};
enum { CLOCK_REALTIME = 0, CLOCK_MONOTONIC = 1 };
int clock_gettime(int clock, struct timespec *ts);
int nanosleep(const struct timespec *wait, struct timespec *left);
struct tm *localtime_r(const time_t *t, struct tm *tm);
time_t mktime(struct tm *tm);
#endif
