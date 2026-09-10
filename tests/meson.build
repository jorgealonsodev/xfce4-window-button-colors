# Each test binary links lc_core only ("tests/ linking lc_core only") —
# no GTK, no panel, no xfconf, no libwnck dependency reaches this
# directory.

test_guard = executable(
  'test-guard',
  'test-guard.c',
  dependencies: [lc_core_dep],
)
test('guard', test_guard)

test_color = executable(
  'test-color',
  'test-color.c',
  dependencies: [lc_core_dep],
)
test('color', test_color)

test_css = executable(
  'test-css',
  'test-css.c',
  dependencies: [lc_core_dep],
)
test('css', test_css)

test_paths = executable(
  'test-paths',
  'test-paths.c',
  dependencies: [lc_core_dep],
)
test('paths', test_paths)

test_winid = executable(
  'test-winid',
  'test-winid.c',
  dependencies: [lc_core_dep],
)
test('winid', test_winid)

test_winstore = executable(
  'test-winstore',
  'test-winstore.c',
  dependencies: [lc_core_dep],
)
test('winstore', test_winstore)

test_restart = executable(
  'test-restart',
  'test-restart.c',
  dependencies: [lc_core_dep],
)
test('restart', test_restart)
