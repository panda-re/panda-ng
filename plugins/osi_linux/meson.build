configure_file(input : 'kernelinfo.conf',
  output : 'kernelinfo.conf',
  copy: true)

dependencies += dependency('libcurl')

sources += files('osi_linux.cpp',
                 'default_profile.cpp',
                 'kernel_2_4_x_profile.cpp',
                 'kernelinfo_downloader.cpp',
                 'utils/kernelinfo/kernelinfo_read.c')