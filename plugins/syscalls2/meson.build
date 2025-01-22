dso_info = run_command('find', 'generated', '-name', 'dso_info_*.c').stdout().strip().split('\n')
syscall_switch = run_command('find', 'generated', '-name', 'syscall_switch_*.cpp').stdout().strip().split('\n')

sources += files(dso_info) + files(syscall_switch) + files('syscalls2.cpp')