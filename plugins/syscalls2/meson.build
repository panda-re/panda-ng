dso_info = run_command('find', 'generated', '-name', 'dso_info_*.c',
                        check: true,
                        capture: true).stdout().strip().split('\n')

foreach dso: dso_info
    dso_basename = dso.split('/')[-1].split('.')[0]

    shared_library('syscalls2_'+dso_basename,
        dso
    )
endforeach

# I don't think there's harm to capturing OABI syscalls on new arm guests, so let's leave this on
# CFLAGS += -DCAPTURE_ARM_OABI
syscall_switch = run_command(
                'find', 'generated', '-name', 'syscall_switch_*.cpp',
                check: true,
                capture: true).stdout().strip().split('\n')
plugin_cflags += ['-DCAPTURE_ARM_OABI']

sources += files('syscalls2.cpp', 'syscalls2_info.c') + files(syscall_switch)