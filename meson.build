project(
  'xfce4-window-button-colors', 'c',
  version: '0.1.0',
  license: 'GPL-2.0-or-later',
  default_options: [
    'c_std=c11',
    'warning_level=2',
  ],
)

gettext_package = meson.project_name()

# --- Core dependencies (glib/gio ONLY) ------------------------------------
#
# src/core/ is the pure, GTK-free layer. It is built as a static library
# that depends only on glib-2.0 and gio-2.0. No GTK, no libxfce4panel, no
# xfconf, no libwnck dependency may ever be added to lc_core: doing so
# would let GTK/panel/wnck-only symbols slip into "pure" code without any
# compiler error, silently destroying the only testable seam in the
# project. CI enforces this by scanning liblc-core.a for undefined
# gtk_/xfconf_/wnck_ symbols.
glib_dep = dependency('glib-2.0', version: '>= 2.56')
gio_dep = dependency('gio-2.0', version: '>= 2.56')

lc_core_inc = include_directories('src/core')

lc_core_sources = files(
  'src/core/lc-guard.c',
  'src/core/lc-color.c',
  'src/core/lc-css.c',
  'src/core/lc-paths.c',
  'src/core/lc-winid.c',
  'src/core/lc-winstore.c',
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

# --- GTK glue (src/glue/) --------------------------------------------------
#
# GTK (and, for this module, libwnck) enter the tree only here and never
# under src/core/. This is the module entry point (src/glue/module.c),
# the sole owner of the CSS provider priority and attach/detach lifecycle
# (src/glue/lc-provider.{c,h}), the tasklist window-button detection and
# tagging hook (src/glue/lc-tasklist.{c,h}), the per-button colour submenu
# (src/glue/lc-menu.{c,h}), and the colour dialog / screen eyedropper
# (src/glue/lc-dialogs.{c,h}).
gtk3_dep = dependency('gtk+-3.0', version: '>= 3.22')
gmodule_dep = dependency('gmodule-2.0')
wnck_dep = dependency('libwnck-3.0')

# The single decision point that picks the GtkCssProvider priority
# literal, compiled into lc-provider.c ONLY (via -DLC_PROVIDER_PRIORITY=...)
# as a plain preprocessor define. No other target may receive this define
# or name a priority constant. Empirically confirmed by the H0-b
# standalone harness (docs/spike-findings.md): APPLICATION already wins
# cleanly against the panel's own competing widget-level provider.
provider_priority_opt = get_option('provider_priority')
if provider_priority_opt == 'user'
  lc_provider_priority_define = 'GTK_STYLE_PROVIDER_PRIORITY_USER'
else
  lc_provider_priority_define = 'GTK_STYLE_PROVIDER_PRIORITY_APPLICATION'
endif

lc_glue_sources = files(
  'src/glue/module.c',
  'src/glue/lc-provider.c',
  'src/glue/lc-tasklist.c',
  'src/glue/lc-menu.c',
  'src/glue/lc-dialogs.c',
)

lc_glue_c_args = [
  '-DLC_PROVIDER_PRIORITY=' + lc_provider_priority_define,
  '-DGETTEXT_PACKAGE="' + gettext_package + '"',
  '-DLOCALEDIR="' + (get_option('prefix') / get_option('localedir')) + '"',
]

# GTK3's own module directory (matches the install location used by
# every other GTK3 module on this system, e.g.
# /usr/lib/x86_64-linux-gnu/gtk-3.0/modules/libcanberra-gtk3-module.so).
gtk3_module_dir = gtk3_dep.get_variable(pkgconfig: 'libdir') / 'gtk-3.0' / 'modules'

lc_module = shared_module(
  'xfce4-window-button-colors',
  lc_glue_sources,
  dependencies: [lc_core_dep, gtk3_dep, gmodule_dep, wnck_dep],
  c_args: lc_glue_c_args,
  install: true,
  install_dir: gtk3_module_dir,
)

# --- Localization (po/) ----------------------------------------------------
i18n = import('i18n')
subdir('po')

subdir('tests')
