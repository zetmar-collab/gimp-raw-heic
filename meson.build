project('gimp-raw-heic', 'c', 'cpp',
  version : '0.9.0',
  license : 'GPL-3.0-or-later',
  default_options : ['c_std=c11', 'cpp_std=c++17', 'buildtype=release',
                     'warning_level=2'])

cc = meson.get_compiler('c')

gimp_dep   = dependency('gimp-3.0', version : '>=3.0')
gimpui_dep = dependency('gimpui-3.0', version : '>=3.0')
gegl_dep   = dependency('gegl-0.4')
heif_dep   = dependency('libheif', version : '>=1.17')
lcms_dep   = dependency('lcms2')
gexiv2_dep = dependency('gexiv2')

# LibRaw is linked statically so the plug-in folder needs no extra DLL;
# its own dependencies (zlib, libjpeg, lcms2, libomp, libc++) ship with GIMP.
libraw_dep = dependency('libraw_r', static : get_option('static_libraw'))
extra_deps = []
if host_machine.system() == 'windows'
  # static LibRaw uses ntohs()/htonl() for byte swapping (no networking)
  extra_deps += cc.find_library('ws2_32')
endif

conf = configuration_data()
conf.set_quoted('PLUGIN_VERSION', meson.project_version())
conf.set_quoted('GETTEXT_PACKAGE', 'gimp-raw-heic')
conf.set_quoted('PLUGIN_NAME', 'gimp-raw-heic')
configure_file(output : 'config.h', configuration : conf)

sources = files(
  'src/plugin.c',
  'src/settings.c',
  'src/raw-develop.c',
  'src/profiles.c',
  'src/image.c',
  'src/raw-load.c',
  'src/raw-dialog.c',
  'src/util.c',
  'src/heif-load.c',
  'src/redevelop.c',
  'src/adjust.c',
  'src/batch.c',
  'src/denoise.c',
)

link_args = []
if host_machine.system() == 'windows'
  link_args += ['-mwindows', '-static-libgcc']
endif

executable('gimp-raw-heic', sources,
  dependencies : [gimp_dep, gimpui_dep, gegl_dep, heif_dep, lcms_dep,
                  gexiv2_dep, libraw_dep] + extra_deps,
  c_args : ['-DLIBRAW_NODLL', '-D_USE_MATH_DEFINES', '-fopenmp', '-Wno-unused-parameter'],
  link_args : link_args,
  # LibRaw is C++: link with the C++ driver
  link_language : 'cpp',
  install : false)

subdir('po')
