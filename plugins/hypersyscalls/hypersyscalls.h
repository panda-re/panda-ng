#ifndef __SYSCALLS_HC
#define __SYSCALLS_HC

#define IGLOO_HYP_SETUP_SYSCALL 0x1337
#define IGLOO_HYP_SYSCALL_ENTER 0x1338
#define IGLOO_HYP_SYSCALL_RETURN 0x1339


// BEGIN_PYPANDA_NEEDS_THIS -- do not delete this comment bc pypanda
// api autogen needs it.  And don't put any compiler directives
// between this and END_PYPANDA_NEEDS_THIS except includes of other
// files in this directory that contain subsections like this one.

/**
 * This stores information about a current syscall
 */

#define MAX_ARGS 7

// add 1 if struct syscall changes
#define SYSCALL_HC_KNOWN_MAGIC 0x1234
struct syscall {
	uint64_t known_magic;
	uint64_t nr;
	uint64_t args[MAX_ARGS];
	uint64_t pc;
	int64_t retval;
	uint64_t skip_syscall;
	uint64_t task;
    uint64_t name_ptr; 
};

/**
 * This stores information about the prototype of a syscall
 */
struct syscall_prototype {
    char *name;
    uint64_t syscall_nr;
    int nargs;
    const char* types[MAX_ARGS];
    const char* names[MAX_ARGS];
};


struct syscall_hook;
typedef void (*syscall_cb_t)(CPUState *cpu, const struct syscall_prototype *syscall,
                             const struct syscall *sysret, struct syscall_hook *hook);

typedef uint32_t ID;

struct syscall_hook {
    ID id;              // Unique id
    bool on_enter;      // true if hook should trigger on entry
    bool on_return;     // true if hook should trigger on return
    bool on_all;        // true if hook should trigger on all known syscalls
    bool on_unknown;    // true if hook should trigger on unknown syscalls
    char name[128];     // name must fully match
    bool enabled;       // true if hook is enabled
    syscall_cb_t cb;    // callback function
};

enum sysret_type {
    RETURN_OK = 0,
    RETURN_SKIP = 1, // skip the syscall on enter
};

void register_syscall_cb(struct syscall_hook *hook);

// END_PYPANDA_NEEDS_THIS -- do not delete this comment!

#endif