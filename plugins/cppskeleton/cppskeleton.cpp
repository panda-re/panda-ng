/* PANDABEGINCOMMENT
 * 
 * Authors:
 *  Tim Leek               tleek@ll.mit.edu
 *  Ryan Whelan            rwhelan@ll.mit.edu
 *  Joshua Hodosh          josh.hodosh@ll.mit.edu
 *  Michael Zhivich        mzhivich@ll.mit.edu
 *  Brendan Dolan-Gavitt   brendandg@gatech.edu
 * 
 * This work is licensed under the terms of the GNU GPL, version 2. 
 * See the COPYING file in the top-level directory. 
 * 
PANDAENDCOMMENT */
// This needs to be defined before anything is included in order to get
// the PRIx64 macro
#define __STDC_FORMAT_MACROS

#include "panda.h"


extern "C" {
bool init_plugin(void *);
void uninit_plugin(void *);
}

void before_block_exec(CPUState* cpu, TranslationBlock* tb);

void before_block_exec(CPUState* cpu, TranslationBlock* tb)
{
    std::cout << "before_block_exec" << std::endl;
}

bool init_plugin(void* self)
{
    panda_cb pcb = { .before_block_exec = before_block_exec };
    panda_register_callback(self, PANDA_CB_START_BLOCK_EXEC, pcb);

    return true;
}

void uninit_plugin(void* self) { }
