plugin_list = run_command('cat', 'config.panda', check: true, capture: true).stdout().strip().split('\n')
fake_libc_include = '../utils/fake_libc_include'
plugin_ext = {}

# First pass: Generate extension headers
foreach plugin: plugin_list
    if fs.exists(join_paths(plugin, plugin+'_int.h'))
        build = meson.current_build_dir()
        in_h =  plugin / (plugin+'_int.h')
        out_h =  plugin+'_ext.h'
        
        # Store the custom target in the dictionary
        plugin_ext += {plugin: custom_target(plugin+'_ext_h',
            input: [in_h],
            output: [out_h],
            install_dir: 'plugin/' + plugin,
            install: true,
            command: [python_exe, '../utils/scripts/apigen.py',
                '@INPUT@', 
                join_paths('plugins', plugin, out_h),
                '-I' + fake_libc_include])}
    endif
endforeach

# Second pass: Build plugins
foreach plugin: plugin_list
    if plugin.strip() == '' or plugin.startswith('#')
        continue
    endif
    build = meson.current_build_dir()
    plugin_build = build / plugin
    
    # message('building plugin ' + plugin)
    
    c_args = [
        '-DCOMPILING_PER_TARGET',
    ]
    cpp_args = [
        '-fpermissive',
        '-Wno-sign-compare',
        '-Wno-error',
        '-Wno-invalid-offsetof',
    ]
    includes = [
        include_directories(plugin),
        include_directories('../include'),
        include_directories('.'),
        panda_ng_include,
    ]
    link_args = [
        '-L' + build,
    ]
    dependencies = [glib]
    
    sources = []
    if fs.exists(join_paths(plugin, plugin+'_int.h'))
        # Use the stored custom target from the dictionary
        dependencies += declare_dependency(sources: plugin_ext[plugin])
    endif
    
    if fs.exists(join_paths(plugin, 'meson.build'))
        plugin_cflags = []
        subdir(plugin)
        foreach target: target_dirs
            # message('building plugin @0@ for target @1@'.format(plugin, target))
            config_target = keyval.load(join_paths(configs, target + '.mak'))
            if 'TARGET_BASE_ARCH' not in config_target
                config_target += {'TARGET_BASE_ARCH': config_target['TARGET_ARCH']}
            endif
            tgt = target.split('-')[0]

            # also has a meson file, but just for changes
            cargo_toml = join_paths(plugin, 'Cargo.toml')
            if fs.exists(cargo_toml)
                # Rust plugins
                rust_build_cmd = [bin_cargo, 'build', 
                            '--no-default-features',
                            '--features', tgt,
                            '--manifest-path', '@INPUT0@',
                            '--target-dir', '@BUILD_ROOT@/plugins/rust/@0@/@1@'.format(plugin,tgt),
                            ]

                if get_option('plugin_debug_rust')
                    rust_build_cmd += ['--debug']
                    output_so = '@BUILD_ROOT@/plugins/rust/@0@/@1@/debug/lib@0@.so'.format(plugin,tgt)
                else
                    rust_build_cmd += ['--release']
                    output_so = '@BUILD_ROOT@/plugins/rust/@0@/@1@/release/lib@0@.so'.format(plugin,tgt)
                endif

                tgt1 = custom_target('panda-@0@_@1@'.format(plugin, tgt),
                    input: [cargo_toml]+ sources,
                    install: true,
                    build_by_default: true,
                    build_always_stale: true,
                    install_dir: 'plugin/' + plugin,
                    output: ['rust_@0@_@1@'.format(plugin,tgt)],
                    command: rust_build_cmd,
                    )
                
                if target == 'mips'
                    rust_tgt = 'mipsel-softmmu'
                else
                    rust_tgt = target
                endif
                custom_target('panda-@0@_@1@_final'.format(plugin, tgt),
                    input: [tgt1],
                    build_by_default: true,
                    build_always_stale: true,
                    output: ['libpanda-@0@_@1@'.format(plugin, rust_tgt)+'.so'],
                    install: true,
                    install_dir: 'plugin/' + plugin,
                    command: [bin_cp, output_so, '@OUTPUT0@'],
                )
            else
                # C/C++ plugins
                target_c_args = [
                    '-DCONFIG_TARGET="@0@-config-target.h"'.format(target),
                    '-DCONFIG_DEVICES="@0@-config-devices.h"'.format(target),
                    '-DPLUGIN_NAME="@0@"'.format(plugin),
                    '-DPANDA_TGT=@0@'.format(tgt),
                    '-D_GNU_SOURCE',
                    '-DTARGET_@0@=1'.format(config_target['TARGET_ARCH'].to_upper())
                ]
                target_link_args = ['-Lpanda-'+target]

                filtered_sources = []
                foreach f: sources
                    fpath = f.full_path()
                    if fpath.endswith(plugin + '.c') or fpath.endswith(plugin + '.cpp')
                        main_src = f
                    else
                        filtered_sources += f
                    endif
                endforeach

                if filtered_sources.length() == 0
                    shared_library('panda-@0@_@1@'.format(plugin, target),
                        sources,
                        c_args: c_args + target_c_args + plugin_cflags,
                        cpp_args: c_args + target_c_args + cpp_args + plugin_cflags + ['-DPLUGIN_MAIN'],
                        link_args: link_args + target_link_args,
                        include_directories: includes,
                        dependencies: dependencies,
                        override_options: ['b_lundef=false', 'werror=false'])
                else
                    main_obj = static_library('panda-@0@_@1@_main'.format(plugin,target),
                    main_src,
                    c_args: c_args + target_c_args + plugin_cflags,
                    cpp_args: c_args + target_c_args + cpp_args + plugin_cflags + ['-DPLUGIN_MAIN'],
                    link_args: link_args + target_link_args,
                    include_directories: includes,
                    dependencies: dependencies,
                    override_options: ['b_lundef=false', 'werror=false'])

                    shared_library('panda-@0@_@1@'.format(plugin, target),
                        filtered_sources,
                        c_args: c_args + target_c_args + plugin_cflags,
                        cpp_args: c_args + target_c_args + cpp_args + plugin_cflags,
                        link_args: link_args + target_link_args,
                        include_directories: includes,
                        link_with: main_obj,
                        dependencies: dependencies,
                        override_options: ['b_lundef=false', 'werror=false'])
                endif
        endif            
        endforeach
    else
        error('meson.build not found in plugin ' + plugin)
    endif
endforeach