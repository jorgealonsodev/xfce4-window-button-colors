/* lc-color.h — RGBA colour parsing and rendering. Links glib only. */
#ifndef LC_COLOR_H
#define LC_COLOR_H

#include <glib.h>

G_BEGIN_DECLS

typedef struct
{
  guint8 r;
  guint8 g;
  guint8 b;
  guint8 a;
} LcColor;

/* Parses an 8-digit "#rrggbbaa" hex string (case-insensitive) into *out.
 * Returns FALSE without touching *out for NULL, a missing '#', a wrong
 * length, or any non-hex-digit character. */
gboolean lc_color_parse (const gchar *text, LcColor *out);

/* Renders the CSS "rgba(r, g, b, a)" declaration value, with alpha as a
 * decimal fraction of color->a / 255 rounded to two decimal places.
 * Caller owns the returned string (g_free). Returns NULL if color is NULL. */
gchar *lc_color_to_css (const LcColor *color);

/* Renders the canonical lowercase "#rrggbbaa" hex form.
 * Caller owns the returned string (g_free). Returns NULL if color is NULL. */
gchar *lc_color_to_hex (const LcColor *color);

G_END_DECLS

#endif /* LC_COLOR_H */
