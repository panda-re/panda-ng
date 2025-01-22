plugin_list = run_command('cat', 'config.panda').stdout().strip().split('\n')
fake_libc_include = '../utils/fake_libc_include'

foreach plugin: plugin_list
    if fs.exists(join_paths(plugin, plugin+'_int.h'))
        message('building plugin ' +plugin+ ' with apigen')
        build = meson.current_build_dir()

        plugin_build = build + '/' + plugin
        message(build)
        run_command('mkdir', '-p', plugin_build)
        
        in_h = meson.current_source_dir() + '/' + plugin + '/'+plugin+'_int.h'
        out_h = plugin_build+'/'+plugin+'_ext.h'

        cmd = [python_exe, '../utils/scripts/apigen.py', 
                in_h, 
                out_h, 
                '-I' + fake_libc_include]
        message(in_h, out_h)
        message(run_command(cmd).stderr())
    endif
endforeach

foreach plugin: plugin_list
    if plugin.strip() == '' or plugin.startswith('#')
        continue
    endif
    build = meson.current_build_dir()
    plugin_build = build + '/' + plugin
    
    message('building plugin ' +plugin)
    
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
    ]

    link_args = [
        '-L'+build,
    ]
    
    sources = []
    if fs.exists(join_paths(plugin, plugin+'_int.h'))
        out_h = plugin_build+'/'+plugin+'_ext.h'
        sources += out_h
    endif


    # is this meson?
    if fs.exists(join_paths(plugin, 'meson.build'))
        subdir(plugin)

        foreach target: target_dirs
            message('building plugin @0@ for target @1@'.format(plugin, target))
            config_target = keyval.load(panda_path + '/configs/targets' / target + '.mak')
            if 'TARGET_BASE_ARCH' not in config_target
                config_target += {'TARGET_BASE_ARCH': config_target['TARGET_ARCH']}
            endif
            message(config_target)
            target_c_args = [
                    '-DCONFIG_TARGET="@0@-config-target.h"'.format(target),
                    '-DCONFIG_DEVICES="@0@-config-devices.h"'.format(target),
                    '-DPLUGIN_NAME="@0@"'.format(plugin),
                    '-DPANDA_TGT=@0@'.format(config_target['TARGET_ARCH']),
                    '-D_GNU_SOURCE',
            ]
            target_link_args = ['-Lpanda-'+target]

            shared_library('panda-@0@_@1@'.format(plugin, target),
                sources,
                c_args: c_args + target_c_args,
                cpp_args: c_args + target_c_args + cpp_args,
                link_args: link_args + target_link_args,
                include_directories: includes,
                dependencies: [glib],
                override_options: ['b_lundef=false', 'werror=false'])
        endforeach
    else
        assert('meson.build not found in plugin ' +plugin)
    endif
endforeach