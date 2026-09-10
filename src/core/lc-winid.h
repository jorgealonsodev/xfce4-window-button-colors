/* lc-winid.h — X11 window-id (XID) validation, parsing, and rendering for
 * the tasklist window-coloring path.
 *
 * This is the window-identity counterpart of the launcher-era lc-id.{c,h}
 * (superseded, kept untouched, not extended). A window's XID has no
 * recycling-mitigation concept analogous to lc-id's plugin-id scheme: XIDs
 * are not reused within a live session in any way this project needs to
 * guard against, so unlike lc-id this module carries no fingerprint
 * concept — see lc-winstore.h's fail-safe lc_winstore_reconcile() for how
 * the window path instead prunes purely against a live-window snapshot.
 *
 * Links glib only.
 */
#ifndef LC_WINID_H
#define LC_WINID_H

#include <glib.h>

G_BEGIN_DECLS

/* An X11 XID is a CARD32 on the wire, stored here as a gulong (matching
 * wnck_window_get_xid()'s return type). 0 is X11's "None" sentinel value
 * and is never a valid window id. */
gboolean lc_winid_is_valid (gulong xid);

/* Parses a decimal, non-negative integer from text (e.g. "138412036") into
 * *out. Returns FALSE without touching *out for NULL/empty text, a value
 * with a leading '+'/'-' sign, embedded/leading/trailing whitespace or any
 * other non-digit character, a value that does not fit in a gulong, or a
 * syntactically valid value that is not lc_winid_is_valid() (i.e. "0"). */
gboolean lc_winid_parse (const gchar *text, gulong *out);

/* Renders the canonical decimal string form, e.g. "138412036".
 * Caller owns the returned string (g_free). Returns NULL for an invalid
 * (lc_winid_is_valid() == FALSE) xid. */
gchar *lc_winid_to_string (gulong xid);

/* Renders the CSS-class form used to tag a tasklist window button:
 * "lc-win-<decimal xid>", e.g. "lc-win-138412036". A decimal digit string
 * is always a valid trailing CSS class/identifier fragment, so unlike the
 * launcher-era wm_class-keyed spike prototype, no character sanitization
 * is ever needed here.
 * Caller owns the returned string (g_free). Returns NULL for an invalid
 * xid. */
gchar *lc_winid_css_class (gulong xid);

G_END_DECLS

#endif /* LC_WINID_H */
