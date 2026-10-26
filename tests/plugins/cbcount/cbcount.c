/* A C plugin that only counts callbacks: the C baseline for tests/bench_cb.py, which measures a
 * C, a bare cffi and a pandare2-decorated callback in one process. Nothing is registered until
 * Python turns a callback on (a registered memory callback instruments accesses even while
 * disabled, so off unregisters); Python reads the counts through the functions below (cffi on the
 * plugin's library: see tests/bench_cb.py). Built by the flake as packages.<system>.cbcount;
 * packages.<system>.test-plugins is panda-ng's plugins plus this one (PANDA_PLUGIN_DIR). */
#include "panda.h"

enum { CB_BBE, CB_VMAR, CB_N };

static void *plugin;
static uint64_t counts[CB_N];
static panda_cb cbs[CB_N];
static const panda_cb_type types[CB_N] = { PANDA_CB_BEFORE_BLOCK_EXEC, PANDA_CB_VIRT_MEM_AFTER_READ };

/* The images run one vCPU; with more, counts would race (and only be approximate). */
static void bbe(CPUState *cpu, TranslationBlock *tb) { counts[CB_BBE]++; }
static void vmar(CPUState *cpu, uint64_t pc, uint64_t addr, size_t size, uint8_t *buf)
{
    counts[CB_VMAR]++;
}

/* which: 0 before_block_exec, 1 virt_mem_after_read. */
uint64_t cbcount_get(int which) { return which >= 0 && which < CB_N ? counts[which] : 0; }

void cbcount_reset(void)
{
    for (int i = 0; i < CB_N; i++) {
        counts[i] = 0;
    }
}

/* on: register that callback; off: unregister all of this plugin's callbacks. */
void cbcount_enable(int which, int on)
{
    if (!on) {
        panda_unregister_callbacks(plugin);
    } else if (which >= 0 && which < CB_N) {
        panda_register_callback(plugin, types[which], cbs[which]);
    }
}

bool init_plugin(void *self)
{
    plugin = self;
    cbs[CB_BBE].before_block_exec = bbe;
    cbs[CB_VMAR].virt_mem_after_read = vmar;
    return true;
}

void uninit_plugin(void *self) {}
