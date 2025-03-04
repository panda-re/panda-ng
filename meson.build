project('panda', ['c', 'cpp'],
        meson_version: '>=1.6.1'
        )

configs = 'configs'

keyval = import('keyval')
fs = import('fs')

target_dirs = get_option('targets')

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

bin_cargo = find_program(
    'cargo',
    native: true,
    required: true,
    version: '>=1.63',
)
bin_cp = find_program(
        'cp',
        native: true,
        required: true,
)

if fs.exists('local_packages/panda-ng')
    panda_ng_include = include_directories('local_packages')
elif fs.exists('/usr/include/panda-ng')
    panda_ng_include = include_directories('/usr/include')
else
    assert(false, 'panda-ng not found. Install it from pandare.deb or put it in local packages')
endif

subdir('plugins')
