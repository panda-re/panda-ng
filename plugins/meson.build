plugin_list = run_command('cat', 'config.panda').stdout().strip().split('\n')
fake_libc_include = '../utils/fake_libc_include'
plugin_ext = {}

# First pass: Generate extension headers
foreach plugin: plugin_list
    if fs.exists(join_paths(plugin, plugin+'_int.h'))
        build = meson.current_build_dir()
        
        in_h = meson.current_source_dir() / plugin / (plugin+'_int.h')
        out_h = plugin+'_ext.h'
        
        # Store the custom target in the dictionary
        plugin_ext += {plugin: custom_target(plugin+'_ext_h',
            output: [out_h],
            input: [in_h],
            install_dir: plugin,
            command: [python_exe, '../utils/scripts/apigen.py', 
                '@INPUT@', 
                '@OUTPUT@', 
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
    
    message('building plugin ' + plugin)
    
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
        '-L' + build,
    ]
    dependencies = [glib]
    
    sources = []
    if fs.exists(join_paths(plugin, plugin+'_int.h'))
        # Use the stored custom target from the dictionary
        dependencies += declare_dependency(sources: plugin_ext[plugin])
    endif
    
    if fs.exists(join_paths(plugin, 'meson.build'))
        subdir(plugin)
        foreach target: target_dirs
            message('building plugin @0@ for target @1@'.format(plugin, target))
            config_target = keyval.load(panda_path + '/configs/targets' / target + '.mak')
            if 'TARGET_BASE_ARCH' not in config_target
                config_target += {'TARGET_BASE_ARCH': config_target['TARGET_ARCH']}
            endif
            
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
                dependencies: dependencies,
                override_options: ['b_lundef=false', 'werror=false'])
        endforeach
    else
        error('meson.build not found in plugin ' + plugin)
    endif
endforeach