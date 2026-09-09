/* lc-id.c — see lc-id.h */
#include "lc-id.h"

gboolean
lc_id_is_valid (gint id)
{
  return id >= 0;
}

gboolean
lc_id_parse (const gchar *text, gint *out)
{
  gint64 value;
  gchar *endptr = NULL;

  if (text == NULL || text[0] == '\0')
    return FALSE;

  /* Digits only: reject signs, embedded/leading/trailing whitespace, and
   * any other non-digit character. g_ascii_strtoll() would silently accept
   * leading whitespace and a leading '+'/'-', so validate byte-by-byte
   * before delegating the actual conversion. */
  for (const gchar *p = text; *p != '\0'; p++)
    {
      if (!g_ascii_isdigit (*p))
        return FALSE;
    }

  value = g_ascii_strtoll (text, &endptr, 10);
  if (endptr == NULL || *endptr != '\0')
    return FALSE;

  if (value < 0 || value > G_MAXINT)
    return FALSE;

  if (out != NULL)
    *out = (gint) value;

  return TRUE;
}

gchar *
lc_id_selector_new (gint id)
{
  if (!lc_id_is_valid (id))
    return NULL;

  return g_strdup_printf ("#launcher-%d #launcher-arrow", id);
}
