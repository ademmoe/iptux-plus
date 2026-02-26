iptux_sources = files(['iptux.cpp', 'iptux_crash_utils.cpp'])
executable('iptux',
    iptux_sources,
    include_directories: inc,
    dependencies: [gtk_dep, jsoncpp_dep, thread_dep, sigc_dep],
    link_with: [libiptux],
    export_dynamic: true,
    install: true,
    link_args: ['-ldl'],
)

gtk4_dep = dependency('gtk4', version: '>=4.0')
adw_dep = dependency('libadwaita-1', version: '>=1.0')
thread_dep = dependency('threads')

iptux_plus_sources = files(['iptux_plus.cpp', 'iptux_crash_utils.cpp'])
executable('iptux-plus',
    iptux_plus_sources,
    include_directories: inc,
    dependencies: [gtk4_dep, adw_dep, jsoncpp_dep, thread_dep, sigc_dep],
    link_with: [libiptux4, libiptux_core, libiptux_utils],
    export_dynamic: true,
    install: true,
    link_args: ['-ldl'],
)
