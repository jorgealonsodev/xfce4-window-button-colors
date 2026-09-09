/* lc-guard.c — see lc-guard.h */
#include "lc-guard.h"

#include <string.h>

gboolean
lc_guard_should_activate (const gchar *prgname)
{
  if (prgname == NULL || prgname[0] == '\0')
    return FALSE;

  return g_strcmp0 (prgname, "xfce4-panel") == 0;
}
