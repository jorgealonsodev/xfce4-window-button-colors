/* lc-color.c — see lc-color.h */
#include "lc-color.h"

#include <string.h>

/* State-derivation constants (design.md D19). Colour blend fractions are
 * stronger for :active than :hover so a pressed launcher reads as a larger
 * change; alpha blend fractions likewise. The pivots keep headroom at the
 * luminance extremes, where a flat lightness bump would otherwise be a
 * near no-op. */
static const gdouble LC_STATE_HOVER_MIX = 0.18;
static const gdouble LC_STATE_ACTIVE_MIX = 0.32;
static const gdouble LC_STATE_HOVER_ALPHA_MIX = 0.20;
static const gdouble LC_STATE_ACTIVE_ALPHA_MIX = 0.35;
static const gdouble LC_STATE_LIGHT_PIVOT = 0.85;
static const gdouble LC_STATE_DARK_PIVOT = 0.15;

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

static guint8
blend_channel_toward (guint8 base, guint8 target, gdouble mix)
{
  gdouble blended = base + ((gdouble) target - (gdouble) base) * mix;

  return (guint8) CLAMP (blended + 0.5, 0.0, 255.0);
}

LcColor
lc_color_derive_state (const LcColor *base, LcState state)
{
  LcColor out = { 0, 0, 0, 0 };
  gdouble luminance;
  gdouble mix, alpha_mix;
  guint8 target;

  if (base == NULL)
    return out;

  /* Cheap perceptual weighted average on the stored sRGB bytes. We only
   * need a direction (light vs. dark), not colorimetric accuracy. */
  luminance = (0.2126 * base->r + 0.7152 * base->g + 0.0722 * base->b) / 255.0;

  if (state == LC_STATE_HOVER)
    {
      mix = LC_STATE_HOVER_MIX;
      alpha_mix = LC_STATE_HOVER_ALPHA_MIX;
      /* Hover targets white unless the base is already very light, in
       * which case white would give no headroom, so target black instead. */
      target = (luminance > LC_STATE_LIGHT_PIVOT) ? 0x00 : 0xFF;
    }
  else /* LC_STATE_ACTIVE */
    {
      mix = LC_STATE_ACTIVE_MIX;
      alpha_mix = LC_STATE_ACTIVE_ALPHA_MIX;
      /* Active targets black unless the base is already very dark, in
       * which case black would give no headroom, so target white instead. */
      target = (luminance < LC_STATE_DARK_PIVOT) ? 0xFF : 0x00;
    }

  out.r = blend_channel_toward (base->r, target, mix);
  out.g = blend_channel_toward (base->g, target, mix);
  out.b = blend_channel_toward (base->b, target, mix);

  /* Alpha always blends toward fully opaque (0xFF), never toward transparent,
   * so a hover/active colour never becomes harder to see than the base. This
   * is a no-op at full opacity, preserving a deliberately opaque choice. */
  out.a = blend_channel_toward (base->a, 0xFF, alpha_mix);

  return out;
}
