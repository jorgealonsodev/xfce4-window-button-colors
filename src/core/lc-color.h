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

/* The two derived pointer-feedback states a base colour can be blended
 * towards (design.md D19). LC_STATE_NORMAL is deliberately absent: the base
 * colour itself already represents that state and is never derived. */
typedef enum
{
  LC_STATE_HOVER,
  LC_STATE_ACTIVE
} LcState;

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

/* Derives the hover or active feedback colour for a base colour
 * (design.md D19): blends the RGB channels toward white or black by a
 * fixed fraction, picking the target by the base's luminance so pure white
 * and pure black both keep headroom to move, and partially blends alpha
 * toward opaque so the state stays visible even on a near-transparent base.
 * Pure function, no GTK. Passing NULL for base returns fully transparent
 * black. */
LcColor lc_color_derive_state (const LcColor *base, LcState state);

G_END_DECLS

#endif /* LC_COLOR_H */
