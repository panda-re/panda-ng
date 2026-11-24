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

// Hook functions must be of this type
typedef bool (*hook_func_t)(CPUState *, TranslationBlock *, struct hook* h);


typedef union hooks_panda_cb {
    void (*before_tcg_codegen)(CPUState *env, TranslationBlock *tb, struct hook*);
    void (*before_block_translate)(CPUState *env, target_ptr_t pc, struct hook*);
    void (*block_translate)(CPUState *env, struct qemu_plugin_tb *tb, struct hook*);
    void (*after_block_translate)(CPUState *env, TranslationBlock *tb, struct hook*);
    bool (*before_block_exec_invalidate_opt)(CPUState *env, TranslationBlock *tb, struct hook*);
    void (*before_block_exec)(CPUState *env, TranslationBlock *tb, struct hook*);
    void (*after_block_exec)(CPUState *env, TranslationBlock *tb, uint8_t exitCode, struct hook*);
    void (*start_block_exec)(CPUState *env, TranslationBlock *tb, struct hook*);
    void (*end_block_exec)(CPUState *env, TranslationBlock *tb, struct hook*);
} hooks_panda_cb;

enum kernel_mode{
    MODE_ANY,
    MODE_KERNEL_ONLY,
    MODE_USER_ONLY,
};

typedef struct hook {
    uint64_t addr;
    uint64_t asid;
    panda_cb_type type;
    hooks_panda_cb cb;
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
