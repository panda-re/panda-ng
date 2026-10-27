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
bin_sh = find_program(
        'sh',
        native: true,
        required: true,
)
bin_cp = find_program(
        'cp',
        native: true,
        required: true,
)

# The plugins build against the headers libpanda-ng generates from the engine (panda-ng/). The
# flake links them into local_packages/; without them the plugins are skipped rather than
# failing the build.
panda_ng_include = []
panda_ng_dir = ''
foreach d : ['local_packages', '/usr/local/include', '/usr/include']
    if panda_ng_include.length() == 0 and fs.exists(d / 'panda-ng')
        panda_ng_include = [include_directories(d)]
        panda_ng_dir = d / 'panda-ng'
    endif
endforeach

# Each plugin links against its target's engine (libpanda-<target>.so), so it has a DT_NEEDED on
# it and binds to it even when the engine is loaded RTLD_LOCAL next to another target's engine
# (pandare2 does that). The engine is the lib/ next to the headers' include/ (the flake's
# local_packages/panda-ng links to <engine>/include/panda-ng), unless engine_libdir says otherwise.
engine_libdir = get_option('engine_libdir')
if engine_libdir == '' and panda_ng_dir != ''
    engine_libdir = run_command(python_exe, '-c',
        'import os, sys; print(os.path.realpath(os.path.join(sys.argv[1], "..", "..", "lib")))',
        meson.current_source_dir() / panda_ng_dir, check: true).stdout().strip()
endif

if panda_ng_include.length() > 0
    subdir('plugins')
else
    warning('no PANDA plugin API in local_packages, /usr/local/include (the engine) or ' +
            '/usr/include: plugins not built')
endif
