/* lc-color.c — see lc-color.h */
#include "lc-color.h"

#include <string.h>

static gboolean
hex_nibble (gchar c, guint8 *out)
{
  if (c >= '0' && c <= '9')
    {
      *out = (guint8) (c - '0');
      return TRUE;
    }
  if (c >= 'a' && c <= 'f')
    {
      *out = (guint8) (10 + c - 'a');
      return TRUE;
    }
  if (c >= 'A' && c <= 'F')
    {
      *out = (guint8) (10 + c - 'A');
      return TRUE;
    }
  return FALSE;
}

static gboolean
hex_byte (const gchar *p, guint8 *out)
{
  guint8 hi, lo;

  if (!hex_nibble (p[0], &hi) || !hex_nibble (p[1], &lo))
    return FALSE;

  *out = (guint8) ((hi << 4) | lo);
  return TRUE;
}

gboolean
lc_color_parse (const gchar *text, LcColor *out)
{
  LcColor c;

  if (text == NULL || text[0] != '#' || strlen (text) != 9)
    return FALSE;

  if (!hex_byte (text + 1, &c.r) ||
      !hex_byte (text + 3, &c.g) ||
      !hex_byte (text + 5, &c.b) ||
      !hex_byte (text + 7, &c.a))
    return FALSE;

  if (out != NULL)
    *out = c;

  return TRUE;
}

gchar *
lc_color_to_css (const LcColor *color)
{
  gdouble alpha;

  if (color == NULL)
    return NULL;

  alpha = color->a / 255.0;

  return g_strdup_printf ("rgba(%u,%u,%u,%.2f)",
                           (guint) color->r, (guint) color->g, (guint) color->b,
                           alpha);
}

gchar *
lc_color_to_hex (const LcColor *color)
{
  if (color == NULL)
    return NULL;

  return g_strdup_printf ("#%02x%02x%02x%02x",
                           (guint) color->r, (guint) color->g,
                           (guint) color->b, (guint) color->a);
}
