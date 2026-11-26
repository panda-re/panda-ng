/* Engine address hooks from C for tests/hookeng.py (built with cc into a shared library, loaded
 * with cffi): hooks inserted and removed through panda/address-hook.h by host threads and by
 * hook callbacks on the vCPU threads, with no Python in the way.
 *
 *   storm: he_storm_start(c0, c1, threads) starts `threads` host threads that insert and remove
 *          hooks at c0 and c1 in a loop, and puts a permanent hook at c0 whose callback inserts
 *          or removes a hook at c1 on every hit. Every transient hook notes when its removal
 *          returned; a call that starts more than SLACK_NS after that is a violation (the
 *          engine allows a call from a pass that was already under way). he_storm_stop() joins.
 *   churn: he_churn_start(hot, base, per) puts a permanent hook at `hot` whose callback, on every
 *          hit, removes the `per` hooks the previous hit inserted and inserts `per` new ones at
 *          unused addresses base + k * 4096 + 64 (one page each): transient sites on ever new
 *          pages, as per-process hooks under ASLR make.
 */
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

struct CPUState;
typedef struct PandaAddressHook PandaAddressHook;
typedef void (*hook_fn)(struct CPUState *, uint64_t, void *);
typedef PandaAddressHook *(*insert_fn)(uint64_t, hook_fn, void *);
typedef void (*remove_fn)(PandaAddressHook *);

#define SLACK_NS 200000000ull           /* 200 ms: a descheduled vCPU thread */
#define MAXT 8
#define MAXPER 64

static insert_fn ins;
static remove_fn rem;
static uint64_t counts[8], violations, removed_late_max;
static volatile bool stop;
static pthread_t thr[MAXT];
static int nthr;
static uint64_t s_c0, s_c1;

typedef struct Trans {
    PandaAddressHook *h;
    uint64_t removed_ns;                /* 0: live */
} Trans;

static uint64_t now_ns(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static void bump(int i)
{
    __atomic_add_fetch(&counts[i], 1, __ATOMIC_RELAXED);
}

void he_setup(void *insert, void *remove)
{
    ins = (insert_fn)insert;
    rem = (remove_fn)remove;
}

uint64_t he_get(int i)
{
    return i == -1 ? __atomic_load_n(&violations, __ATOMIC_RELAXED)
         : i == -2 ? __atomic_load_n(&removed_late_max, __ATOMIC_RELAXED)
         : __atomic_load_n(&counts[i], __ATOMIC_RELAXED);
}

/* A transient hook's callback: counts, and checks it was not removed long before. */
static void trans_hit(struct CPUState *cpu, uint64_t pc, void *opaque)
{
    Trans *t = opaque;
    uint64_t r = __atomic_load_n(&t->removed_ns, __ATOMIC_ACQUIRE);

    bump(1);
    if (r) {
        uint64_t late = now_ns() - r;

        if (late > SLACK_NS) {
            __atomic_add_fetch(&violations, 1, __ATOMIC_RELAXED);
        }
        uint64_t m = __atomic_load_n(&removed_late_max, __ATOMIC_RELAXED);
        while (late > m && !__atomic_compare_exchange_n(&removed_late_max, &m, late, false,
                                                        __ATOMIC_RELAXED, __ATOMIC_RELAXED)) {
        }
    }
}

/* Insert a transient hook at @pc; its record is never freed (a removed hook's callback may
 * still read it from a pass under way). */
static Trans *trans_insert(uint64_t pc)
{
    Trans *t = calloc(1, sizeof(*t));

    t->h = ins(pc, trans_hit, t);
    if (!t->h) {
        __atomic_add_fetch(&violations, 1000000, __ATOMIC_RELAXED);
    }
    return t;
}

static void trans_remove(Trans *t)
{
    rem(t->h);
    __atomic_store_n(&t->removed_ns, now_ns(), __ATOMIC_RELEASE);
}

/* storm: the vCPU side */
static Trans *vcpu_t;                   /* the callbacks' transient hook at c1 */
static pthread_mutex_t vcpu_lock = PTHREAD_MUTEX_INITIALIZER;

static void storm_hit(struct CPUState *cpu, uint64_t pc, void *opaque)
{
    bump(0);
    pthread_mutex_lock(&vcpu_lock);
    if (vcpu_t) {
        trans_remove(vcpu_t);
        vcpu_t = NULL;
    } else {
        vcpu_t = trans_insert(s_c1);
    }
    pthread_mutex_unlock(&vcpu_lock);
}

static void *storm_thread(void *arg)
{
    uintptr_t k = (uintptr_t)arg;
    unsigned n = 0;

    while (!__atomic_load_n(&stop, __ATOMIC_RELAXED)) {
        Trans *t = trans_insert((n + k) & 1 ? s_c0 : s_c1);
        struct timespec ts = {0, 50000 + 10000 * (long)k};

        nanosleep(&ts, NULL);
        trans_remove(t);
        bump(2);
        n++;
    }
    return NULL;
}

static PandaAddressHook *perm;

void he_storm_start(uint64_t c0, uint64_t c1, int threads)
{
    s_c0 = c0;
    s_c1 = c1;
    stop = false;
    perm = ins(c0, storm_hit, NULL);
    nthr = threads < MAXT ? threads : MAXT;
    for (int i = 0; i < nthr; i++) {
        pthread_create(&thr[i], NULL, storm_thread, (void *)(uintptr_t)i);
    }
}

void he_storm_stop(void)
{
    __atomic_store_n(&stop, true, __ATOMIC_RELAXED);
    for (int i = 0; i < nthr; i++) {
        pthread_join(thr[i], NULL);
    }
    nthr = 0;
    rem(perm);
    pthread_mutex_lock(&vcpu_lock);
    if (vcpu_t) {
        trans_remove(vcpu_t);
        vcpu_t = NULL;
    }
    pthread_mutex_unlock(&vcpu_lock);
}

/* churn */
static uint64_t c_base, c_next;
static int c_per;
static Trans *c_live[MAXPER];
static pthread_mutex_t churn_lock = PTHREAD_MUTEX_INITIALIZER;

static void churn_hit(struct CPUState *cpu, uint64_t pc, void *opaque)
{
    bump(3);
    pthread_mutex_lock(&churn_lock);
    for (int i = 0; i < c_per; i++) {
        if (c_live[i]) {
            trans_remove(c_live[i]);
        }
        c_live[i] = trans_insert(c_base + c_next++ * 4096 + 64);
    }
    pthread_mutex_unlock(&churn_lock);
}

void he_churn_start(uint64_t hot, uint64_t base, int per)
{
    c_base = base;
    c_per = per < MAXPER ? per : MAXPER;
    perm = ins(hot, churn_hit, NULL);
}

uint64_t he_churn_inserted(void)
{
    return c_next;
}

/* a plain counting hook (counter 4), for measurements */
static void count_hit(struct CPUState *cpu, uint64_t pc, void *opaque)
{
    bump(4);
}

void *he_count_insert(uint64_t pc)
{
    return ins(pc, count_hit, NULL);
}
