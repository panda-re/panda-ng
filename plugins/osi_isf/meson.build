dependencies += dependency('nlohmann_json')
dependencies += dependency('liblzma')
# PPP calls into osi need its generated header.
dependencies += declare_dependency(sources: plugin_ext['osi'])

sources += files('osi_isf.cpp', 'isf.cpp')
