gtk4_dep = dependency('gtk4', version: '>=4.0')
adw_dep = dependency('libadwaita-1', version: '>=1.0')
jsoncpp_dep = dependency('jsoncpp', version: '>=1.0')
sigc_dep = dependency('sigc++-2.0')

sources4 = files([
    'Application4.cpp',
    'MainWindow4.cpp',
    'Preferences4.cpp',
])

inc4 = include_directories('..', '../api')

libiptux4 = static_library('iptux4',
    sources4,
    dependencies: [gtk4_dep, adw_dep, jsoncpp_dep, sigc_dep],
    link_with: [libiptux_core, libiptux_utils],
    include_directories: inc4,
)
