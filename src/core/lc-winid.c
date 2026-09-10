/* lc-winid.c — see lc-winid.h */
#include "lc-winid.h"

#include <errno.h>

gboolean
lc_winid_is_valid (gulong xid)
{
  return xid != 0;
}

gboolean
lc_winid_parse (const gchar *text, gulong *out)
{
  guint64 value;
  gchar *endptr = NULL;

  if (text == NULL || text[0] == '\0')
    return FALSE;

  /* Reject anything that is not a bare, unsigned decimal digit string
   * before even attempting to parse it: no sign, no whitespace, no
   * embedded junk, no hex/float shape. g_ascii_strtoull() alone would
   * silently accept a leading '+' or trailing garbage via its endptr
   * contract, so the canonical-form check has to happen here instead. */
  for (const gchar *p = text; *p != '\0'; p++)
    {
      if (!g_ascii_isdigit (*p))
        return FALSE;
    }

  errno = 0;
  value = g_ascii_strtoull (text, &endptr, 10);

  if (endptr == NULL || *endptr != '\0' || endptr == text)
    return FALSE;

  if (errno == ERANGE || value > (guint64) G_MAXULONG)
    return FALSE;

  if (!lc_winid_is_valid ((gulong) value))
    return FALSE;

  if (out != NULL)
    *out = (gulong) value;

  return TRUE;
}

gchar *
lc_winid_to_string (gulong xid)
{
  if (!lc_winid_is_valid (xid))
    return NULL;

  return g_strdup_printf ("%lu", xid);
}

gchar *
lc_winid_css_class (gulong xid)
{
  if (!lc_winid_is_valid (xid))
    return NULL;

  return g_strdup_printf ("lc-win-%lu", xid);
}
