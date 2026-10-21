project('panda', ['c', 'cpp'],
        default_options: ['c_std=gnu11', 'cpp_std=gnu++11'],
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

# The plugins build against the engine's PANDA plugin API (<panda/plugin.h>). The Dockerfile
# installs the engine (engine/, qemu_builder) under /usr/local. engine/ does not carry the PANDA
# API yet (pandang's spine and families are still being ported onto it), so without it the
# plugins are skipped rather than failing the build.
panda_ng_include = []
foreach d : ['local_packages', '/usr/local/include', '/usr/include']
    if panda_ng_include.length() == 0 and fs.exists(d / 'panda-ng')
        panda_ng_include = [include_directories(d)]
    endif
endforeach

if panda_ng_include.length() > 0
    subdir('plugins')
else
    warning('no PANDA plugin API in local_packages, /usr/local/include (the engine) or ' +
            '/usr/include: plugins not built')
endif
