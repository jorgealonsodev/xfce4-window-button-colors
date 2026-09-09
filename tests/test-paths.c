/* tests/test-paths.c — RED test for XDG path resolution with $HOME unset
 * (module-lifecycle spec step 3 failure path: one g_warning, no crash).
 */
#include <glib.h>
#include <string.h>
#include "lc-paths.h"

static void
test_paths_config_dir_with_home_set (void)
{
  gchar *saved_home = g_strdup (g_getenv ("HOME"));
  gchar *dir;

  g_setenv ("HOME", "/tmp/lc-test-home", TRUE);

  dir = lc_paths_config_dir ();
  g_assert_nonnull (dir);
  g_assert_cmpstr (dir, ==, "/tmp/lc-test-home/.config/xfce4-launcher-colors");
  g_free (dir);

  if (saved_home != NULL)
    g_setenv ("HOME", saved_home, TRUE);
  else
    g_unsetenv ("HOME");
  g_free (saved_home);
}

static void
test_paths_colors_css_with_home_set (void)
{
  gchar *saved_home = g_strdup (g_getenv ("HOME"));
  gchar *path;

  g_setenv ("HOME", "/tmp/lc-test-home", TRUE);

  path = lc_paths_colors_css ();
  g_assert_nonnull (path);
  g_assert_cmpstr (path, ==, "/tmp/lc-test-home/.config/xfce4-launcher-colors/colors.css");
  g_free (path);

  if (saved_home != NULL)
    g_setenv ("HOME", saved_home, TRUE);
  else
    g_unsetenv ("HOME");
  g_free (saved_home);
}

static void
test_paths_settings_ini_with_home_set (void)
{
  gchar *saved_home = g_strdup (g_getenv ("HOME"));
  gchar *path;

  g_setenv ("HOME", "/tmp/lc-test-home", TRUE);

  path = lc_paths_settings_ini ();
  g_assert_nonnull (path);
  g_assert_cmpstr (path, ==, "/tmp/lc-test-home/.config/xfce4-launcher-colors/settings.ini");
  g_free (path);

  if (saved_home != NULL)
    g_setenv ("HOME", saved_home, TRUE);
  else
    g_unsetenv ("HOME");
  g_free (saved_home);
}

static void
test_paths_config_dir_with_home_unset (void)
{
  gchar *saved_home = g_strdup (g_getenv ("HOME"));
  gchar *dir;

  g_unsetenv ("HOME");

  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "*HOME*");
  dir = lc_paths_config_dir ();
  g_test_assert_expected_messages ();

  g_assert_null (dir);

  if (saved_home != NULL)
    g_setenv ("HOME", saved_home, TRUE);
  g_free (saved_home);
}

static void
test_paths_colors_css_with_home_unset_does_not_crash (void)
{
  gchar *saved_home = g_strdup (g_getenv ("HOME"));
  gchar *path;

  g_unsetenv ("HOME");

  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "*HOME*");
  path = lc_paths_colors_css ();
  g_test_assert_expected_messages ();

  g_assert_null (path);

  if (saved_home != NULL)
    g_setenv ("HOME", saved_home, TRUE);
  g_free (saved_home);
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/paths/config-dir-with-home-set", test_paths_config_dir_with_home_set);
  g_test_add_func ("/paths/colors-css-with-home-set", test_paths_colors_css_with_home_set);
  g_test_add_func ("/paths/settings-ini-with-home-set", test_paths_settings_ini_with_home_set);
  g_test_add_func ("/paths/config-dir-with-home-unset", test_paths_config_dir_with_home_unset);
  g_test_add_func ("/paths/colors-css-with-home-unset-does-not-crash", test_paths_colors_css_with_home_unset_does_not_crash);

  return g_test_run ();
}
