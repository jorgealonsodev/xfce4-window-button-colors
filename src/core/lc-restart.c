/* lc-restart.c — see lc-restart.h */
#include "lc-restart.h"

gchar **
lc_restart_panel_argv (void)
{
  gchar **argv = g_new0 (gchar *, 3);

  argv[0] = g_strdup ("xfce4-panel");
  argv[1] = g_strdup ("-r");
  argv[2] = NULL;

  return argv;
}
