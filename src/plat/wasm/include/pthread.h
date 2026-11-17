/* One thread: a lock is always free, and no thread starts (the runtime's tick is qbrt_wasm.c's). */
#ifndef PTHREAD_H
#define PTHREAD_H
typedef int pthread_mutex_t;
typedef int pthread_t;
#define PTHREAD_MUTEX_INITIALIZER 0
static inline int pthread_mutex_lock(pthread_mutex_t *const m) { return 0; }
static inline int pthread_mutex_trylock(pthread_mutex_t *const m) { return 0; }
static inline int pthread_mutex_unlock(pthread_mutex_t *const m) { return 0; }
static inline int pthread_create(
    pthread_t *const t,
    const void *const attr,
    void *(*const fn)(void *),
    void *const arg
)
{
    return -1;
}
static inline int pthread_join(const pthread_t t, void **const ret) { return 0; }
#endif
