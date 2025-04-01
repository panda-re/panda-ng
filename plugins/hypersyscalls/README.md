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
void register_syscall_cb(struct syscall_hook *cb);

// Syscall hook structure definition
struct syscall_hook {
    const char *name;        // Name of syscall to hook, or NULL for all
    bool on_enter;           // Whether to call on syscall entry
    bool on_return;          // Whether to call on syscall return
    bool on_all;             // Whether to hook all syscalls
    bool enabled;            // Whether hook is enabled
    void *opaque;            // User data
    syscall_cb_t cb;         // Callback function
};

// Callback function type
typedef void (*syscall_cb_t)(CPUState *cpu, 
                          struct syscall_prototype *syscall_info,
                          struct syscall *syscall,
                          struct syscall_hook *hook);
```

Examples
--------

### Registering a Callback

```C
void my_syscall_callback(CPUState *cpu, struct syscall_prototype *syscall_info, 
                         struct syscall *syscall, struct syscall_hook *hook) {
    printf("Syscall %s called with args: %lx, %lx\n", 
           syscall_info->name, syscall->args[0], syscall->args[1]);
}

// Hook all read syscalls
struct syscall_hook read_hook = {
    .name = "sys_read",
    .on_enter = true,
    .on_return = true,
    .on_all = false,
    .enabled = true,
    .opaque = NULL,
    .cb = my_syscall_callback
};

register_syscall_cb(&read_hook);
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

This typically involves either modifying the kernel or loading a kernel module that hooks into the system call mechanism.

Note that the hypercalls plugin only works when the guest OS is specifically instrumented to use it, making it a cooperative approach rather than a purely external one.
