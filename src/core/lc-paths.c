/* lc-paths.c — see lc-paths.h */
#include "lc-paths.h"

gchar *
lc_paths_config_dir (void)
{
  const gchar *home = g_getenv ("HOME");

  if (home == NULL || home[0] == '\0')
    {
      g_warning ("lc_paths_config_dir: HOME is unset; cannot resolve config directory");
      return NULL;
    }

  return g_build_filename (home, ".config", "xfce4-window-button-colors", NULL);
}

gchar *
lc_paths_colors_css (void)
{
  gchar *dir = lc_paths_config_dir ();
  gchar *path;

  if (dir == NULL)
    return NULL;

  path = g_build_filename (dir, "colors.css", NULL);
  g_free (dir);

  return path;
}

gchar *
lc_paths_settings_ini (void)
{
  gchar *dir = lc_paths_config_dir ();
  gchar *path;

  if (dir == NULL)
    return NULL;

  path = g_build_filename (dir, "settings.ini", NULL);
  g_free (dir);

  return path;
}
