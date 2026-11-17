/* Locks on the shared memory, for the runtime's tick (dev/host/tick.c): the page's thread fires it
   with trylock, which never waits (its thread may not); the module's waits on the word. No thread
   starts: the page's timer is the tick's thread. */
#ifndef PTHREAD_H
#define PTHREAD_H
typedef int pthread_mutex_t;
typedef int pthread_t;
#define PTHREAD_MUTEX_INITIALIZER 0
static inline int pthread_mutex_trylock(pthread_mutex_t *const m)
{
    int free = 0;
    return __atomic_compare_exchange_n(m, &free, 1, 0, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED) ? 0 : 16;
}
static inline int pthread_mutex_lock(pthread_mutex_t *const m)
{
    while (pthread_mutex_trylock(m))
        __builtin_wasm_memory_atomic_wait32(m, 1, -1);

    return 0;
}
static inline int pthread_mutex_unlock(pthread_mutex_t *const m)
{
    __atomic_store_n(m, 0, __ATOMIC_RELEASE);
    __builtin_wasm_memory_atomic_notify(m, 1);
    return 0;
}
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
