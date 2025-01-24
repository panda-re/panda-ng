project('panda', ['c', 'cpp'])

panda_path = meson.current_source_dir()+'/qemu'
build = panda_path+'/build'
configs = 'configs'

keyval = import('keyval')
fs = import('fs')

config_host = keyval.load(join_paths(configs, 'panda-ng.mak'))

cpu = host_machine.cpu_family()
if cpu == 'x86'
  host_arch = 'i386'
else
  host_arch = cpu
endif


target_dirs = config_host['TARGET_DIRS'].split()

glib_req_ver = '>=2.66.0'
glib_pc = dependency('glib-2.0', version: glib_req_ver, required: true,
                    method: 'pkg-config')
glib_cflags = []
gmodule = dependency('gmodule-no-export-2.0', version: glib_req_ver, required: true,
                       method: 'pkg-config')

glib = declare_dependency(dependencies: [glib_pc, gmodule],
                          compile_args: glib_cflags,
                          version: glib_pc.version())

python_exe = find_program('python3', 'python')
subdir('plugins')
