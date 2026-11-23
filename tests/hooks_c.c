/* C hook functions for tests/hooks.py, which builds this file with cc into a shared library and
 * loads it with cffi: the functions run on the vCPU threads with no Python in the way, through
 * the hooks plugin's add_hook (struct hook from plugins/hooks/hooks_int_fns.h). A hook's context
 * is the index of the counter it bumps.
 *
 *   hookc_count   counts each call; its type is BEFORE_TCG_CODEGEN
 *   hookc_once    counts, then disables itself
 *   hookc_churn   counts, then adds a hookc_once hook (counter 2) at hookc_churn_addr: the hooks
 *                 at that address are replaced while other vCPUs run them
 */
#include <stdbool.h>
#include <stdint.h>

typedef struct CPUState CPUState;
typedef struct TranslationBlock TranslationBlock;
typedef uint64_t target_ptr_t;
typedef int panda_cb_type;
struct qemu_plugin_tb;
#include "../plugins/hooks/hooks_int_fns.h"

#define NCOUNT 8
static uint64_t counts[NCOUNT];
static uint64_t bad_args;
static void (*add)(struct hook *);
static uint64_t churn_addr;
static int codegen_type;          /* PANDA_CB_BEFORE_TCG_CODEGEN, from the engine's headers */

static void bump(CPUState *cpu, TranslationBlock *tb, struct hook *h)
{
    uintptr_t i = (uintptr_t)h->context;

    if (!cpu || !tb || i >= NCOUNT) {
        __atomic_add_fetch(&bad_args, 1, __ATOMIC_RELAXED);
        return;
    }
    __atomic_add_fetch(&counts[i], 1, __ATOMIC_RELAXED);
}

void hookc_count(CPUState *cpu, TranslationBlock *tb, struct hook *h)
{
    bump(cpu, tb, h);
}

void hookc_once(CPUState *cpu, TranslationBlock *tb, struct hook *h)
{
    bump(cpu, tb, h);
    h->enabled = false;
}

void hookc_churn(CPUState *cpu, TranslationBlock *tb, struct hook *h)
{
    struct hook n = {0};

    bump(cpu, tb, h);
    n.addr = churn_addr;
    n.asid = 0;
    n.type = codegen_type;
    n.cb.before_tcg_codegen = hookc_once;
    n.km = MODE_USER_ONLY;
    n.enabled = true;
    n.context = (void *)2;
    add(&n);
}

void hookc_setup(void (*add_hook)(struct hook *), int type, uint64_t addr)
{
    add = add_hook;
    codegen_type = type;
    churn_addr = addr;
}

/* Add a hook from C: type BEFORE_TCG_CODEGEN, user mode, any asid. fn: 0 count, 1 once,
 * 2 churn. */
void hookc_add(uint64_t addr, int fn, int counter)
{
    void (*fns[])(CPUState *, TranslationBlock *, struct hook *) = {
        hookc_count, hookc_once, hookc_churn };
    struct hook h = {0};

    h.addr = addr;
    h.asid = 0;
    h.type = codegen_type;
    h.cb.before_tcg_codegen = fns[fn];
    h.km = MODE_USER_ONLY;
    h.enabled = true;
    h.context = (void *)(uintptr_t)counter;
    add(&h);
}

uint64_t hookc_get(int i)
{
    return i < 0 ? __atomic_load_n(&bad_args, __ATOMIC_RELAXED)
                 : i < NCOUNT ? __atomic_load_n(&counts[i], __ATOMIC_RELAXED) : 0;
}
