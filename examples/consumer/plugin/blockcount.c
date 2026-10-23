/* An out-of-tree PANDA plugin: counts executed blocks; Python reads the count with
 * blockcount_get() (see ../count.py). */
#include "panda.h"

static uint64_t blocks;

uint64_t blockcount_get(void) { return blocks; }

static void start_block(CPUState *cpu, TranslationBlock *tb)
{
    blocks++;
}

bool init_plugin(void *self)
{
    panda_register_callback(self, PANDA_CB_START_BLOCK_EXEC,
                            (panda_cb){ .start_block_exec = start_block });
    return true;
}

void uninit_plugin(void *self)
{
    printf("blockcount: %llu blocks\n", (unsigned long long) blocks);
}
