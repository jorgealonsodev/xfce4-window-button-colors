# Each test binary links lc_core only (design.md: "tests/ linking lc_core
# only") — no GTK, no panel, no xfconf dependency reaches this directory.

test_guard = executable(
  'test-guard',
  'test-guard.c',
  dependencies: [lc_core_dep],
)
test('guard', test_guard)

test_id = executable(
  'test-id',
  'test-id.c',
  dependencies: [lc_core_dep],
)
test('id', test_id)

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
