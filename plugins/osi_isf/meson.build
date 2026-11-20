dependencies += dependency('nlohmann_json')
dependencies += dependency('liblzma')
# PPP calls into osi need its generated header.
dependencies += declare_dependency(sources: plugin_ext['osi'])

sources += files('osi_isf.cpp', 'facts.cpp', 'lifecycle.cpp', 'isf.cpp', 'caps.cpp')

# The capability report without a guest, for the host (installed beside the plugin).
executable('osi_isf_check', files('osi_isf_check.cpp', 'isf.cpp', 'caps.cpp'),
    dependencies: [dependency('nlohmann_json'), dependency('liblzma')],
    install: true,
    install_dir: 'plugin/osi_isf')
