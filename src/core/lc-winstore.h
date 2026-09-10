/* lc-winstore.h — LcWinStore: XID -> colour map for the tasklist
 * window-coloring path (load/parse, write, and full-document rendering).
 *
 * This is the window-identity counterpart of the launcher-era
 * lc-store.{c,h} (superseded, deleted). It deliberately does NOT reuse or
 * extend lc-store's on-disk shape or its id+fingerprint reconciliation
 * model: a window's XID needs no recycling-mitigation fingerprint (see
 * lc-winid.h). lc_winstore_reconcile() below is a pure, snapshot-based
 * prune with a fundamentally different, and directionally OPPOSITE,
 * fail-safe shape from the launcher-era lc_store_reconcile_entry()
 * sketch — see that function's own doc comment for the full rationale.
 * The shipped module does NOT call it from a startup snapshot, though:
 * see lc_winstore_reconcile()'s own doc comment for why a snapshot taken
 * too early caused real data loss in production, and src/glue/lc-tasklist.c
 * for the event-driven ("window-closed" signal) pruning that replaced it.
 *
 * Parser discipline is carried over from lc-store.c's hard-won lessons:
 * - the schema banner is a format-migration marker, NEVER a validity gate
 *   that can discard a user's colours;
 * - a record whose rules disagree with their own marker is malformed and
 *   is skipped WHOLE, leaving every other record unaffected;
 * - at most one g_warning() is emitted per lc_winstore_load() call, no
 *   matter how many distinct issues the file has;
 * - a missing, unreadable, non-UTF-8, or structurally malformed file
 *   degrades to an EMPTY store, never a crash, never an outward error.
 *
 * LcWinStore's rendered CSS document is produced by lc_winstore_render(),
 * the ONLY constructor of LcDocument on this path (design D16 carried
 * over from the launcher era): nothing else may hand the CSS provider
 * (src/glue/lc-provider.c) anything but a rendered, reconciled store.
 *
 * Links glib and gio only (gio for g_file_get_contents/g_file_set_contents
 * error reporting) — never gtk, xfconf, or wnck.
 */
#ifndef LC_WINSTORE_H
#define LC_WINSTORE_H

#include <glib.h>
#include "lc-color.h"
#include "lc-document.h"

G_BEGIN_DECLS

typedef struct _LcWinStore LcWinStore;

/* Returns a newly allocated, empty store. Caller owns it (lc_winstore_free()). */
LcWinStore *lc_winstore_new (void);

/* Loads and parses `path` into a newly allocated LcWinStore. NEVER
 * returns NULL and never crashes or aborts. A missing file, an unreadable
 * file, non-UTF-8 content, or content with no recognizable records at all
 * degrades to an EMPTY store.
 *
 * A missing file is the ordinary first-run case — nothing has ever been
 * saved yet — and is logged at g_debug, never g_warning, and does NOT
 * count toward the budget below: a normal first panel start must stay
 * quiet. Every OTHER degrade path (unreadable, non-UTF-8, malformed) is
 * still budgeted to at most ONE g_warning() per call, regardless of how
 * many distinct such issues the file has.
 *
 * The generated-file banner is a format-migration marker, never a
 * validity gate: its absence contributes to the one-warning budget above
 * but never discards otherwise-parseable entries.
 *
 * The parser is record-oriented: one comment marker naming the xid
 * (see lc-winstore.c for the literal pattern) followed by every rule up
 * to the next marker or EOF. Only the first rule (the base rule)
 * must be exactly ".lc-win-<xid> { ... }" for the marker's own xid, and
 * only its background-color is read back — the hover/active colors are
 * always re-derived at render time, never parsed. Every rule after the
 * base must be a state rule "<base>:<state>" whose body carries EXACTLY
 * ONE declaration, background-color. A record violating any of this is
 * malformed and is skipped whole.
 *
 * path == NULL is treated the same as an unreadable file.
 * Caller owns the returned store (lc_winstore_free()). */
LcWinStore *lc_winstore_load (const gchar *path);

/* Writes store's persisted form (the same marker+rule text lc_winstore_load()
 * parses back) to `path`, creating or truncating it. Creates path's parent
 * directory (and its parents) first if it does not already exist yet —
 * on a genuinely fresh install nothing has ever been saved before, so
 * there is nothing else that would have created it. Returns TRUE on
 * success. On failure, returns FALSE and emits exactly one g_warning();
 * the caller decides whether/how to surface that further. store and path
 * must both be non-NULL. */
gboolean lc_winstore_save (const LcWinStore *store, const gchar *path);

/* Sets (inserting or overwriting) the colour for `xid`. No-op, silently,
 * if store is NULL, xid is not lc_winid_is_valid(), or color is NULL. */
void lc_winstore_set (LcWinStore *store, gulong xid, const LcColor *color);

/* Removes the colour for `xid`, if any. Returns TRUE if an entry was
 * removed, FALSE if there was nothing to remove (including when store is
 * NULL or xid is invalid). */
gboolean lc_winstore_unset (LcWinStore *store, gulong xid);

/* Returns TRUE iff store holds a colour for `xid`. FALSE for a NULL
 * store or an invalid xid. */
gboolean lc_winstore_has (const LcWinStore *store, gulong xid);

/* Fetches the colour stored for `xid` into *out. Returns FALSE, leaving
 * *out untouched, if there is no entry for xid (including a NULL store,
 * an invalid xid, or a NULL out). */
gboolean lc_winstore_get (const LcWinStore *store, gulong xid, LcColor *out);

/* Number of entries currently held by store. store must be non-NULL. */
guint lc_winstore_size (const LcWinStore *store);

/* Pure prune against a caller-supplied live-XID snapshot (design: fail
 * SAFE, which for this path means the OPPOSITE direction from the
 * launcher-era lc_store_reconcile_entry() sketch — read this carefully,
 * the two are deliberately not analogous):
 *
 *   - `live_list_obtained` == FALSE means the caller could not obtain a
 *     live-window snapshot AT ALL (e.g. no WnckScreen/display available).
 *     In that case this function KEEPS EVERY ENTRY UNCHANGED and returns
 *     0 — an inability to check is never treated as absence.
 *   - `live_list_obtained` == TRUE but `n_live` == 0 gets the EXACT SAME
 *     treatment: KEEPS EVERY ENTRY UNCHANGED and returns 0. This is not
 *     an oversight — it was a real, measured production data-loss bug.
 *     libwnck populates its window list asynchronously; a caller that
 *     reads it too early sees a successfully-obtained, genuinely-empty
 *     list that is, a fraction of a second later, 29 windows. This
 *     function has no way to distinguish that from a session that
 *     truly has zero windows open, and must not guess: it treats
 *     "obtained but empty" as "not obtained" for pruning purposes,
 *     always. The asymmetry is deliberate: keeping a few stale entries
 *     costs nothing, wrongly dropping live ones costs the user colours
 *     they cannot get back.
 *   - `live_list_obtained` == TRUE AND `n_live` > 0 is the only case
 *     that may prune anything. Every entry whose xid is NOT in that
 *     snapshot is dropped — removed ONLY when its absence is positively
 *     confirmed against a successfully obtained, non-empty list, never
 *     merely inferred.
 *
 * NOTE for callers: even the third case above is a SNAPSHOT, and a
 * snapshot taken too early can be non-empty yet still incomplete (e.g.
 * partway through libwnck's async population), which this function
 * cannot detect from `n_live` alone. The shipped module does not call
 * this from any startup/idle snapshot for exactly that reason — see
 * src/glue/lc-tasklist.c's "window-closed"-signal-driven pruning, which
 * only ever prunes on positive per-window evidence. This function is
 * kept, pure and fully tested, for a caller that has independently
 * verified its snapshot is both non-empty and complete.
 *
 * Also see the "never write when nothing changed" rule this pairs with:
 * a caller MUST NOT call lc_winstore_save() after a reconcile that
 * returned 0 — a session that changes nothing must not rewrite the
 * user's file (or touch its mtime), which also bounds the blast radius
 * of any future bug here: a wrong in-memory prune that never reaches
 * disk is recoverable.
 *
 * Returns the number of entries dropped. store must be non-NULL;
 * live_xids may be NULL only when n_live == 0. Pure: performs no I/O and
 * touches nothing but `store`. */
guint lc_winstore_reconcile (LcWinStore *store, gboolean live_list_obtained,
                              const gulong *live_xids, gsize n_live);

/* Renders the full colors.css document for store: the schema banner,
 * then one marker comment and its rule triplet (via lc_css_rule_new())
 * per entry, in a stable order. This is the SOLE constructor of
 * LcDocument on the window path: nothing else in the tree may produce
 * one, so lc-provider.c can never be handed anything but a rendered,
 * reconciled store.
 *
 * store must be non-NULL. Caller owns the returned document
 * (lc_document_free()). */
LcDocument *lc_winstore_render (const LcWinStore *store);

/* Frees store and every entry it holds. Safe to call with NULL. */
void lc_winstore_free (LcWinStore *store);

G_END_DECLS

#endif /* LC_WINSTORE_H */
