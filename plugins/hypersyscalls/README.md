Plugin: hypersyscalls
===========

Summary
-------

`hypersyscalls` provides a mechanism for monitoring and modifying system calls through hypercalls in PANDA. It allows guest operating systems to communicate their system call information to the hypervisor, enabling analysis and manipulation of system calls without relying on OS-specific introspection techniques.

The plugin works by establishing a hypercall-based protocol between the guest and PANDA, where the guest provides detailed information about system calls at entry and return points. This approach offers better reliability across different kernel versions and configurations compared to traditional introspection methods.

How It Works
-----------

1. The guest operating system provides system call definitions to PANDA via hypercalls.
2. When a system call occurs, the guest notifies PANDA at both entry and exit points.
3. Registered callbacks can then inspect and modify system call arguments and return values.

This cooperative approach requires minimal guest-side modifications but provides robust system call interception capabilities.

Guest Requirements
----------------

To use the hypersyscalls plugin, the guest needs to:

1. Provide system call definitions through the `IGLOO_HYP_SETUP_SYSCALL` hypercall.
2. Notify PANDA on system call entry using `IGLOO_HYP_SYSCALL_ENTER`.
3. Notify PANDA on system call exit using `IGLOO_HYP_SYSCALL_RETURN`.
4. Provide task_struct->comm offset using `IGLOO_HYP_SETUP_TASK_COMM` (if using process name filtering).

System call definitions are provided in JSON format, containing:
- System call name
- System call number
- Argument names and types

Dependencies
------------

`hypersyscalls` depends on the `hypercaller` plugin, which provides the basic hypercall infrastructure.

APIs and Callbacks
------------------

The plugin provides a simple API for registering callbacks on system calls:

```C
// Register a callback for system call events
void register_syscall_cb(struct syscall_hook *hook);

// Syscall hook structure definition
struct syscall_hook {
    ID id;                              // Unique id
    bool enabled;                       // true if hook is enabled
    bool on_enter;                      // true if hook should trigger on entry
    bool on_return;                     // true if hook should trigger on return
    bool on_all;                        // true if hook should trigger on all known syscalls
    bool on_unknown;                    // true if hook should trigger on unknown syscalls
    char name[128];                     // name must fully match
    bool comm_filter_enabled;           // true if comm_filter is enabled
    char comm_filter[16];               // process name must fully match
    bool filter_args_enabled;           // true if filter_args is enabled
    bool filter_arg[MAX_ARGS];          // true if hook should filter this argument
    uint64_t arg_filter[MAX_ARGS];      // argument to filter on
    syscall_cb_t cb;                    // callback function
};

// Callback function type
typedef void (*syscall_cb_t)(CPUState *cpu, 
                          const struct syscall_prototype *syscall,
                          const struct syscall *sysret, 
                          struct syscall_hook *hook);
```

### Filtering Options

The hypersyscalls plugin supports filtering syscalls based on:

1. **Process Name Filtering**: Set `comm_filter_enabled` to `true` and provide a process name in the `comm_filter` field. The hook will only trigger for syscalls from processes whose name matches exactly.

2. **Argument Filtering**: Set `filter_args_enabled` to `true` and then for each argument you want to filter on, set `filter_arg[i]` to `true` and provide the value to match in `arg_filter[i]`. The hook will only trigger when all specified arguments match the provided values.

These filters can be combined to create highly targeted hooks that only execute for specific processes and syscall argument combinations.

### Validation Conditions

When registering a syscall hook, the following conditions are enforced:

1. You cannot set both `on_unknown` and `on_all` to true at the same time.
2. You cannot set `on_unknown` to true and specify a syscall name at the same time.
3. At least one of `on_enter` or `on_return` must be true for the hook to be effective.
4. When specifying a syscall name, it must match a syscall in the registered table.

If any of these conditions are not met, the hook will not be properly registered or an error message will be displayed.

Examples
--------

### Registering a Callback

```C
void my_syscall_callback(CPUState *cpu, const struct syscall_prototype *syscall, 
                         const struct syscall *sysret, struct syscall_hook *hook) {
    printf("Syscall %s called with args: %lx, %lx\n", 
           syscall->name, sysret->args[0], sysret->args[1]);
}

// Hook all read syscalls
struct syscall_hook read_hook = {
    .name = "sys_read",
    .on_enter = true,
    .on_return = true,
    .on_all = false,
    .on_unknown = false,
    .enabled = true,
    .comm_filter_enabled = false,
    .filter_args_enabled = false,
    .cb = my_syscall_callback
};

register_syscall_cb(&read_hook);
```

### Filtering by Process Name

```C
// Hook only read syscalls from 'bash'
struct syscall_hook bash_read_hook = {
    .name = "sys_read",
    .on_enter = true,
    .on_return = false,
    .on_all = false,
    .on_unknown = false,
    .enabled = true,
    .comm_filter_enabled = true,
    .filter_args_enabled = false,
    .cb = my_syscall_callback
};

// Set comm filter to 'bash'
strncpy(bash_read_hook.comm_filter, "bash", sizeof(bash_read_hook.comm_filter));
register_syscall_cb(&bash_read_hook);
```

### Filtering by Arguments

```C
// Hook only read syscalls for file descriptor 0 (stdin)
struct syscall_hook stdin_read_hook = {
    .name = "sys_read",
    .on_enter = true,
    .on_return = false,
    .on_all = false,
    .on_unknown = false,
    .enabled = true,
    .comm_filter_enabled = false,
    .filter_args_enabled = true,
    .cb = my_syscall_callback
};

// Set filter for first argument (fd) to be 0
stdin_read_hook.filter_arg[0] = true;
stdin_read_hook.arg_filter[0] = 0;
register_syscall_cb(&stdin_read_hook);
```

### Hooking Unknown Syscalls

```C
void unknown_syscall_handler(CPUState *cpu, const struct syscall_prototype *syscall, 
                             const struct syscall *sysret, struct syscall_hook *hook) {
    // Note: syscall will be NULL for unknown syscalls
    printf("Unknown syscall number %ld with args: %lx, %lx\n", 
           sysret->nr, sysret->args[0], sysret->args[1]);
}

// Hook all syscalls not registered in the table
struct syscall_hook unknown_hook = {
    .name = "",  // Empty name since we're using on_unknown
    .on_enter = true,
    .on_return = false,
    .on_all = false,
    .on_unknown = true,
    .enabled = true,
    .comm_filter_enabled = false,
    .filter_args_enabled = false,
    .cb = unknown_syscall_handler
};

register_syscall_cb(&unknown_hook);
```

### Running with hypersyscalls

```bash
$PANDA_PATH/x86_64-softmmu/panda-system-x86_64 -replay foo \
    -panda hypercaller -panda hypersyscalls \
    -panda my_analysis_plugin
```

Guest-side Integration
---------------------

For the guest side, you need to:

1. Create a system for notifying the hypervisor of system calls.
2. Send system call definitions using the `IGLOO_HYP_SETUP_SYSCALL` hypercall.
3. Notify PANDA at system call entry and exit points.
4. Provide task_struct->comm offset using `IGLOO_HYP_SETUP_TASK_COMM` if you want to filter by process name.

This typically involves either modifying the kernel or loading a kernel module that hooks into the system call mechanism.

Note that the hypercalls plugin only works when the guest OS is specifically instrumented to use it, making it a cooperative approach rather than a purely external one.
