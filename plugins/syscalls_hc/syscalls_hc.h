#ifndef __SYSCALLS_HC
#define __SYSCALLS_HC

// BEGIN_PYPANDA_NEEDS_THIS -- do not delete this comment bc pypanda
// api autogen needs it.  And don't put any compiler directives
// between this and END_PYPANDA_NEEDS_THIS except includes of other
// files in this directory that contain subsections like this one.


struct syscall_info {
    int syscall_nr;
    char* name;
    char** types;
    char** args;
};
void initialize_syscalls(struct syscall_info **info, int length);

// END_PYPANDA_NEEDS_THIS -- do not delete this comment!

#endif