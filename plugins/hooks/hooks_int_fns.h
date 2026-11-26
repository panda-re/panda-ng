#ifndef __HOOKS_INT_FNS_H__
#define __HOOKS_INT_FNS_H__

#ifdef __cplusplus
extern "C" {
#endif

// BEGIN_PYPANDA_NEEDS_THIS -- do not delete this comment bc pypanda
// api autogen needs it.  And don't put any compiler directives
// between this and END_PYPANDA_NEEDS_THIS except includes of other
// files in this directory that contain subsections like this one.

#define MAX_PROCNAME_LENGTH 256
#define MAX_PATH_LEN 256
struct hook;

// A hook's function: called right before the guest executes the instruction at the hook's
// address, with the translated block that holds the instruction and the plugin's copy of the hook.
typedef void (*hook_func_t)(CPUState *cpu, TranslationBlock *tb, struct hook *h);

enum kernel_mode{
    MODE_ANY,
    MODE_KERNEL_ONLY,
    MODE_USER_ONLY,
};

typedef struct hook {
    uint64_t addr;
    uint64_t asid;
    hook_func_t cb;
    enum kernel_mode km;
    bool enabled;
    void* context;
} hook;


void add_hook(struct hook* h);
void enable_hooking();
void disable_hooking();

// END_PYPANDA_NEEDS_THIS -- do not delete this comment!

#ifdef __cplusplus
}
#endif
#endif
