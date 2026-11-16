specific_ss.add(
    files(
        'common.c',
        'analysis-effect.c',
        'callbacks.c',
        'cb-support.c',
        'events.c',
        'events-core.c',
        'events-hmp.c',
        'panda_api.c',
        'panda_arch.c',
        'panda_cov.c',
        'panda_fast.c',
        'panda_mem.c',
        'panda_qemu_plugin_helpers.c',
        'rr_control.c',
        'guest-write.c',
        'syscall-pair.c',
        'wrap_ops.c',
    )
)

# Hot paths, compiled per target with their own code generation flags (see the target loop in
# the top-level meson.build): no call-used register zeroing on return, no frame pointer.
panda_hot_ss = ss.source_set()
panda_hot_ss.add(files('syscall-pair-fast.c'))
panda_hot_cflags = cc.get_supported_arguments(['-fzero-call-used-regs=skip',
                                               '-fomit-frame-pointer'])
