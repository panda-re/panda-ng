project('libosi', ['cpp'],
    default_options: ['cpp_std=gnu++14'],
    meson_version: '>=1.6.1'
)

src_root = meson.current_source_dir() + '/src'
offset_src_root = src_root + '/offset'
iohal_src_root = src_root + '/iohal'
osi_src_root = src_root + '/osi'
build = meson.current_build_dir()

iohal_sources = files(
    iohal_src_root + '/memory/pmem_snapshot.cc',
    iohal_src_root + '/translators/virtual_memory_translators.cc',
    iohal_src_root + '/translators/vm_amd64.cc',
    iohal_src_root + '/translators/vm_i386.cc',
    iohal_src_root + '/translators/vm_i386_pae.cc',
    iohal_src_root + '/memory/virtual_memory.cc'
)

offset_sources = files(
    offset_src_root + '/core.cc',
    offset_src_root + '/profiles/win_2000_x86.cpp',
    offset_src_root + '/profiles/win_xpsp2_x86.cpp',
    offset_src_root + '/profiles/win_xpsp3_x86.cpp',
    offset_src_root + '/profiles/win7_sp0_x86.cpp',
    offset_src_root + '/profiles/win7_sp1_x86.cpp',
    offset_src_root + '/profiles/win7_sp0_x64.cpp',
    offset_src_root + '/profiles/win7_sp1_x64.cpp',
    offset_src_root + '/profiles/linux3_16_x86.cpp',
    offset_src_root + '/profiles/linux3_16_x64.cpp'
)

osi_sources = files(
    osi_src_root + '/windows/manager.cc',
    osi_src_root + '/windows/kernel_osi.cc',
    osi_src_root + '/windows/api.cc',
    osi_src_root + '/windows/windows_handles.cc',
    osi_src_root + '/windows/mem_pe/mem_pe.cc',
    osi_src_root + '/linux/api.cc',
    osi_src_root + '/linux/kernel_osi.cc'
)

sources = \
    iohal_sources + \
    offset_sources + \
    osi_sources

includes = include_directories(
    'include'
)

# Build dynamic and static iohal library
libiohal = shared_library('iohal',
    iohal_sources,
    include_directories: includes,
    install: true
)
libiohal_static = static_library('iohal-static',
    iohal_sources,
    include_directories: includes,
    install: true
)

# Build dynamic and static offset library
liboffset = shared_library('offset',
    offset_sources,
    include_directories: includes,
    install: true
)
liboffset_static = static_library('offset-static',
    offset_sources,
    include_directories: includes,
    install: true
)

# Build dynamic and static osi library
libosi = shared_library('osi',
    osi_sources,
    link_with: [libiohal, liboffset],
    include_directories: includes,
    install: true
)
libosi_static = static_library('osi-static',
    osi_sources,
    link_with: [libiohal_static, liboffset_static],
    include_directories: includes,
    install: true
)

# Create dynamic and static dependencies for use as subproject
libiohal_dep = declare_dependency(
    include_directories: includes,
    link_with: libiohal
)
libiohal_static_dep = declare_dependency(
    include_directories: includes,
    link_with: libiohal_static
)
liboffset_dep = declare_dependency(
    include_directories: includes,
    link_with: liboffset
)
liboffset_static_dep = declare_dependency(
    include_directories: includes,
    link_with: liboffset_static
)
libosi_dep = declare_dependency(
    include_directories: includes,
    link_with: libosi
)
libosi_static_dep = declare_dependency(
    include_directories: includes,
    link_with: libosi_static
)
