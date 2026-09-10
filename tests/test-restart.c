/* tests/test-restart.c — lc_restart_panel_argv(): the exact argv vector
 * fed to g_spawn_async() to restart xfce4-panel (design D4).
 *
 * D4's whole point is a vector, never a shell string: g_spawn_async()
 * consumes argv directly and never invokes a shell, so there is no
 * injection seam as long as nothing here ever assembles or delegates to
 * "sh -c ...". The launcher-era threat-matrix rule ("argv vector, never
 * g_spawn_command_line_*, never a shell string") applies here too, and
 * applies harder: this is a GUI action a user clicks, not a fixed CLI
 * invocation. The assertions below exist to pin exactly that: the vector
 * shape (3 elements, NULL-terminated), the literal command and flag, and
 * a scan for a shell escape hatch ("sh"/"-c") or any shell metacharacter
 * in ANY element — a future refactor that quietly swapped in
 * g_spawn_command_line_async() could still produce a vector that LOOKS
 * right by shape alone, so shape assertions by themselves are not
 * enough; this is the test that is actually the point of this file, not
 * decoration around it.
 */
#include <glib.h>
#include <string.h>
#include "lc-restart.h"

/* Characters that carry special meaning to a POSIX shell. None of them
 * may appear in a spawned argv element here: g_spawn_async() never
 * passes this vector through a shell, but this assertion documents and
 * enforces that intent even against a future caller that mistakenly
 * routed it through one. */
static const gchar *SHELL_METACHARACTERS = " \t\n;&|$`\\\"'<>(){}[]*?~#!^";

static void
assert_no_shell_metacharacter (const gchar *element)
{
  g_assert_nonnull (element);
  g_assert_null (strpbrk (element, SHELL_METACHARACTERS));
}

static void
test_restart_panel_argv_has_exactly_three_elements (void)
{
  /* { "xfce4-panel", "-r", NULL } — exactly two real tokens and a
   * terminating NULL, matching g_spawn_async()'s argv contract. Both
   * directions matter: too few elements would drop the flag, and any
   * trailing element after the NULL would never be reached anyway, so
   * the position of the NULL is what actually bounds the vector. */
  gchar **argv = lc_restart_panel_argv ();

  g_assert_nonnull (argv);
  g_assert_nonnull (argv[0]);
  g_assert_nonnull (argv[1]);
  g_assert_null (argv[2]);

  g_strfreev (argv);
}

static void
test_restart_panel_argv_first_element_is_the_panel_binary (void)
{
  gchar **argv = lc_restart_panel_argv ();

  g_assert_cmpstr (argv[0], ==, "xfce4-panel");

  g_strfreev (argv);
}

static void
test_restart_panel_argv_second_element_is_the_restart_flag (void)
{
  gchar **argv = lc_restart_panel_argv ();

  g_assert_cmpstr (argv[1], ==, "-r");

  g_strfreev (argv);
}

static void
test_restart_panel_argv_contains_no_shell_escape_hatch (void)
{
  /* This is the D4 test, not decoration: neither "sh" nor "-c" — the two
   * tokens that would turn this vector into a shell invocation if ever
   * handed to a "sh", "-c", <cmd> style spawn — may appear as an
   * element, and no element may carry a character a shell would treat
   * specially. */
  gchar **argv = lc_restart_panel_argv ();
  guint i;

  for (i = 0; argv[i] != NULL; i++)
    {
      g_assert_cmpstr (argv[i], !=, "sh");
      g_assert_cmpstr (argv[i], !=, "-c");
      assert_no_shell_metacharacter (argv[i]);
    }

  g_strfreev (argv);
}

static void
test_restart_panel_argv_returns_a_fresh_vector_each_call (void)
{
  /* Caller owns the returned vector (g_strfreev()); a second call must
   * not hand back the same backing memory a first caller may already
   * have freed. */
  gchar **first = lc_restart_panel_argv ();
  gchar **second = lc_restart_panel_argv ();

  g_assert_true (first != second);
  g_assert_cmpstr (first[0], ==, second[0]);
  g_assert_cmpstr (first[1], ==, second[1]);

  g_strfreev (first);
  g_strfreev (second);
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/restart/panel-argv-has-exactly-three-elements", test_restart_panel_argv_has_exactly_three_elements);
  g_test_add_func ("/restart/panel-argv-first-element-is-the-panel-binary", test_restart_panel_argv_first_element_is_the_panel_binary);
  g_test_add_func ("/restart/panel-argv-second-element-is-the-restart-flag", test_restart_panel_argv_second_element_is_the_restart_flag);
  g_test_add_func ("/restart/panel-argv-contains-no-shell-escape-hatch", test_restart_panel_argv_contains_no_shell_escape_hatch);
  g_test_add_func ("/restart/panel-argv-returns-a-fresh-vector-each-call", test_restart_panel_argv_returns_a_fresh_vector_each_call);

  return g_test_run ();
}
