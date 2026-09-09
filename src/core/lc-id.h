/* lc-id.h — launcher unique-id validation, parsing, and selector generation
 * (design.md D5). Links glib only.
 */
#ifndef LC_ID_H
#define LC_ID_H

#include <glib.h>

G_BEGIN_DECLS

/* A launcher's xfce4-panel unique id is a non-negative integer. */
gboolean lc_id_is_valid (gint id);

/* Parses a decimal, non-negative integer from text (e.g. "12") into *out.
 * Returns FALSE without touching *out for NULL/empty text, a value with
 * a leading '+'/'-' sign, embedded non-digit characters, leading/trailing
 * whitespace, or a value that does not fit in a gint. */
gboolean lc_id_parse (const gchar *text, gint *out);

/* Returns a newly allocated selector string "#launcher-<id> #launcher-arrow"
 * (design.md D5). The single-level "#launcher-<id>" and the non-existent
 * "#launcher-button" MUST NEVER be produced by this or any other function.
 * Caller owns the returned string (g_free). Returns NULL for an invalid id. */
gchar *lc_id_selector_new (gint id);

G_END_DECLS

#endif /* LC_ID_H */
