/* lc-autoload.h — pure gchar** list transforms for the `/Gtk/Modules`
 * autoload key (design.md D1), plus the orchestration layer that decides
 * how to write that key back without ever touching xfconf itself.
 *
 * `/Gtk/Modules` has no fixed schema type: xfconf may hold it absent, as
 * a scalar string (the shape the README's own `-n -t string -s` command
 * creates for every existing v0.1.0 user), or as an array of strings, and
 * it may already list other applications' GTK modules. D1's whole point
 * is to preserve whichever shape is found and never drop or re-type a
 * third-party entry. This file is split into two layers so that split is
 * itself testable:
 *
 *   - the FIVE pure functions below (normalize/join/is_enabled/add/remove)
 *     operate only on an in-memory, NULL-terminated `gchar **` list — no
 *     xfconf, no I/O, and no knowledge of "shape" at all.
 *   - `LcAutoloadShape`, `LcAutoloadResult`, `LcAutoloadBackend`, and
 *     `lc_autoload_set_enabled()` are the orchestration layer that reads
 *     the shape via the vtable, applies add/remove, and decides the
 *     write-back shape per D1's table. This layer is still xfconf-free —
 *     it is tested against a fake `LcAutoloadBackend` in
 *     tests/test-autoload.c. The real, xfconf-backed implementation of
 *     `LcAutoloadBackend` lives in Phase 4's
 *     src/settings/lc-autoload-xfconf.c, entirely out of this file.
 *
 * `lc_autoload_normalize()` closes the actual bug this design exists to
 * prevent: GTK's own `gtk-modules` value is colon-separated, so a scalar
 * string like "canberra-gtk-module:xfce4-window-button-colors" is TWO
 * entries, not one opaque token. Treating it as one token means `remove`
 * silently no-ops (the exact-match compare never matches the compound
 * string) and `add` appends a duplicate compound entry — both are
 * third-party-entry corruption in disguise, the same failure class D1 as
 * a whole exists to rule out. `lc_autoload_is_enabled()` is exact-match
 * only for the mirror reason: "xfce4-window-button-colors-extra" must
 * never read as "xfce4-window-button-colors" being enabled, or a disable
 * action would target the wrong module.
 *
 * Links glib only. Must never include <gtk/gtk.h>, <xfconf/xfconf.h>, or
 * any libwnck header — CI scans liblc-core.a for undefined gtk_/xfconf_/
 * wnck_ symbols and fails the build if any leaks in here.
 */
#ifndef LC_AUTOLOAD_H
#define LC_AUTOLOAD_H

#include <glib.h>

G_BEGIN_DECLS

/* The on-disk shape `/Gtk/Modules` was found in, carried unchanged from
 * `read` to `write` (D1's "the shape is a witness" choice). ABSENT is
 * the shape `read` reports when the key has never existed at all, and
 * is also what `write` is called with to mean "delete the key entirely"
 * (the write-back decision when the resulting list is empty). UNKNOWN
 * covers any xfconf value type other than a string or a string array —
 * D1 says refuse the write outright rather than guess a conversion. */
typedef enum {
  LC_AUTOLOAD_SHAPE_ABSENT,
  LC_AUTOLOAD_SHAPE_SCALAR,
  LC_AUTOLOAD_SHAPE_ARRAY,
  LC_AUTOLOAD_SHAPE_UNKNOWN
} LcAutoloadShape;

/* Outcome of lc_autoload_set_enabled() (Phase 3). LC_AUTOLOAD_NO_CHANGE
 * means `write` was NOT called at all — the same "never write when
 * nothing changed" rule lc_winstore_reconcile() documents for pruning,
 * applied here to xfconf: enabling an already-enabled module, or
 * disabling an absent one, must not touch the key or its mtime. */
typedef enum {
  LC_AUTOLOAD_OK,
  LC_AUTOLOAD_NO_CHANGE,
  LC_AUTOLOAD_READ_FAILED,
  LC_AUTOLOAD_UNSUPPORTED_SHAPE,
  LC_AUTOLOAD_WRITE_FAILED
} LcAutoloadResult;

/* The xfconf-facing vtable lc_autoload_set_enabled() orchestrates
 * against. Exactly two functions, deliberately: `read` reports the
 * current shape and, for SCALAR/ARRAY, the ALREADY NORMALIZED module
 * list (a real backend calls lc_autoload_normalize() itself on a raw
 * scalar before returning it — this header, not the backend, owns that
 * rule); `write` commits a shape and list back, where shape ABSENT
 * means "delete the key" and `modules` is then ignored. Declaring this
 * struct here — fully, not opaquely — lets one real implementation
 * (xfconf-backed, in src/settings/lc-autoload-xfconf.c, Phase 4) and a
 * fake test implementation (tests/test-autoload.c) both exist against
 * the exact same contract. */
typedef struct {
  gboolean (*read)  (gpointer user_data, LcAutoloadShape *out_shape, gchar ***out_modules);
  gboolean (*write) (gpointer user_data, LcAutoloadShape shape, const gchar *const *modules);
} LcAutoloadBackend;

/* ---- pure list transforms (implemented in this slice) ---------------- */

/* Splits `scalar` on ':' (GTK's own `gtk-modules` separator), trims
 * surrounding whitespace from each piece, and drops pieces that are
 * empty after trimming. NULL and the empty string both normalize to a
 * non-NULL, zero-length (single-NULL-terminator) array — never NULL, so
 * callers can always iterate the result without a NULL check first.
 * Caller owns the returned array (g_strfreev()). */
gchar **lc_autoload_normalize (const gchar *scalar);

/* The inverse of lc_autoload_normalize() for the SCALAR write-back shape
 * (D1): joins `modules` back into one ':'-separated string. `modules`
 * may be NULL, treated the same as a zero-length array. Never returns
 * NULL; a NULL or empty `modules` joins to "". Caller owns the returned
 * string (g_free). */
gchar *lc_autoload_join (const gchar *const *modules);

/* TRUE iff `modules` contains an entry that is byte-for-byte EQUAL to
 * `name` — never a prefix or substring match, so
 * "xfce4-window-button-colors-extra" is never read as our module being
 * enabled. FALSE for a NULL `modules` or `name`, and FALSE when `name`
 * is genuinely absent. */
gboolean lc_autoload_is_enabled (const gchar *const *modules, const gchar *name);

/* Returns a newly allocated copy of `modules` with `name` appended if it
 * is not already present (per lc_autoload_is_enabled()'s exact-match
 * rule), or an unchanged copy if it already is — idempotent, so calling
 * this twice in a row never produces a duplicate entry. Every existing
 * entry's order and content is preserved unchanged. `modules` may be
 * NULL, treated as empty. `name` must be non-NULL. Caller owns the
 * returned array (g_strfreev()). */
gchar **lc_autoload_add (const gchar *const *modules, const gchar *name);

/* Returns a newly allocated copy of `modules` with every entry EQUAL to
 * `name` removed; every other entry survives, byte-for-byte unchanged,
 * in its original order. A no-op (returns an equivalent copy) when
 * `name` is not present — never an error, never a warning. `modules`
 * may be NULL, treated as empty. `name` must be non-NULL. Caller owns
 * the returned array (g_strfreev()). */
gchar **lc_autoload_remove (const gchar *const *modules, const gchar *name);

/* ---- orchestration over the vtable — D1's full shape table ------------ */

/* Reads the current shape and list via `backend->read`, applies
 * lc_autoload_add()/lc_autoload_remove() per `enabled`, decides the
 * write-back shape per D1's table, and calls `backend->write` unless
 * nothing changed. `user_data` is passed through to both vtable calls
 * unmodified.
 *
 * Returns LC_AUTOLOAD_READ_FAILED if `backend->read` reports failure
 * (no write is attempted); LC_AUTOLOAD_UNSUPPORTED_SHAPE if the shape
 * read is neither ABSENT, SCALAR nor ARRAY (no write is attempted);
 * LC_AUTOLOAD_NO_CHANGE if the requested add/remove would not change
 * anything (no write is attempted — enabling an already-enabled module
 * or disabling an absent one both land here); LC_AUTOLOAD_WRITE_FAILED
 * if `backend->write` reports failure; LC_AUTOLOAD_OK otherwise. */
LcAutoloadResult lc_autoload_set_enabled (const LcAutoloadBackend *backend, gpointer user_data,
                                           const gchar *name, gboolean enabled);

G_END_DECLS

#endif /* LC_AUTOLOAD_H */
