/* tests/test-guard.c — RED test for lc_guard_should_activate()
 *
 * Covers module-lifecycle spec "xfce4-panel-Only Process Guard" (RNF-6):
 * exactly "xfce4-panel" activates; every other name, including NULL, the
 * empty string, and near-misses, must not.
 */
#include <glib.h>
#include "lc-guard.h"

static void
test_guard_exact_match_activates (void)
{
  g_assert_true (lc_guard_should_activate ("xfce4-panel"));
}

static void
test_guard_null_does_not_activate (void)
{
  g_assert_false (lc_guard_should_activate (NULL));
}

static void
test_guard_empty_does_not_activate (void)
{
  g_assert_false (lc_guard_should_activate (""));
}

static void
test_guard_other_process_does_not_activate (void)
{
  g_assert_false (lc_guard_should_activate ("gedit"));
}

static void
test_guard_wrapper_name_does_not_activate (void)
{
  /* Near-miss: must not activate inside the wrapper process. */
  g_assert_false (lc_guard_should_activate ("xfce4-panel-wrapper"));
}

static void
test_guard_case_sensitive (void)
{
  g_assert_false (lc_guard_should_activate ("Xfce4-Panel"));
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/guard/exact-match-activates", test_guard_exact_match_activates);
  g_test_add_func ("/guard/null-does-not-activate", test_guard_null_does_not_activate);
  g_test_add_func ("/guard/empty-does-not-activate", test_guard_empty_does_not_activate);
  g_test_add_func ("/guard/other-process-does-not-activate", test_guard_other_process_does_not_activate);
  g_test_add_func ("/guard/wrapper-name-does-not-activate", test_guard_wrapper_name_does_not_activate);
  g_test_add_func ("/guard/case-sensitive", test_guard_case_sensitive);

  return g_test_run ();
}
