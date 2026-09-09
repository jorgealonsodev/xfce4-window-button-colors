project(
  'xfce4-launcher-colors', 'c',
  version: '0.1.0',
  license: 'GPL-2.0-or-later',
  default_options: [
    'c_std=c11',
    'warning_level=2',
  ],
)

# --- Core dependencies (glib/gio ONLY) ------------------------------------
#
# src/core/ is the pure, GTK-free layer (design.md D1). It is built as a
# static library that depends only on glib-2.0 and gio-2.0. No GTK, no
# libxfce4panel, no xfconf dependency may ever be added to lc_core: doing
# so would let GTK-only symbols slip into "pure" code without any compiler
# error, silently destroying the only testable seam in the project.
glib_dep = dependency('glib-2.0', version: '>= 2.56')
gio_dep = dependency('gio-2.0', version: '>= 2.56')

lc_core_inc = include_directories('src/core')

lc_core_sources = files(
  'src/core/lc-guard.c',
  'src/core/lc-id.c',
  'src/core/lc-color.c',
  'src/core/lc-css.c',
  'src/core/lc-paths.c',
)

lc_core = static_library(
  'lc-core',
  lc_core_sources,
  dependencies: [glib_dep, gio_dep],
  include_directories: lc_core_inc,
)

lc_core_dep = declare_dependency(
  link_with: lc_core,
  include_directories: lc_core_inc,
  dependencies: [glib_dep, gio_dep],
)

subdir('tests')
