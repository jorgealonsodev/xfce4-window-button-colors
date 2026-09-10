/* lc-winlist.h — LcWinlist: the pure live/orphan join for the stored-colour
 * management view (design D2, D8).
 *
 * WHY THIS FILE EXISTS AT ALL (design D8, an amendment made during task
 * breakdown, not part of the original plan): the stored-color-management
 * spec marks "Orphaned entry never appears as a row" and "List never
 * marks entries as orphaned under an ambiguous snapshot" as
 * (headless-testable). The original design placed this join entirely in
 * src/settings/ — pure GTK/wnck glue — which left those two scenarios
 * with no unit test, reachable only by manual inspection. That is an
 * artifact contradiction (a spec claiming a headless test exists for
 * logic that, as designed, could not be headless-tested), not a
 * preference, so the join was split the same way D1 already splits
 * lc-autoload: this file owns the truth table and the join itself, and
 * src/settings/lc-winlist-wnck.c (Phase 7) owns the one thing that
 * cannot be pure — capturing the actual libwnck snapshot and the
 * self-XID completeness probe (D2/D3).
 *
 * WHY THE FAIL-SAFE DIRECTION IS WHAT IT IS — READ THIS BEFORE "FIXING" IT:
 * this join is the SECOND caller of the exact hazard that already
 * destroyed real user data in this project. libwnck populates its window
 * list asynchronously: measured on the live session, 0 windows at idle
 * time and 29 windows 1.5 seconds later. lc_winstore_reconcile() was
 * already hardened against believing that early empty snapshot (see its
 * own doc comment in lc-winstore.h for the full incident). Here the
 * consequence of getting it wrong is WORSE, not equal: reconcile() only
 * runs from code paths nothing clicks, but this join feeds a list the
 * USER looks at and a cleanup button the USER clicks. If an ambiguous
 * snapshot were ever read as "no windows are open," every stored colour
 * would be presented as orphaned and the user would delete their own
 * work on this module's own recommendation.
 *
 * The rule this file enforces, therefore, mirrors lc_winstore_reconcile()
 * exactly, not a second, differently-shaped version of it:
 *
 *   - snapshot_ok == FALSE (the caller could not obtain a live snapshot
 *     at all, OR obtained one that came back empty — see
 *     lc_winstore_reconcile()'s identical "obtained-but-empty is treated
 *     as not-obtained" rule; both cases are collapsed into this single
 *     boolean by the wnck-facing caller BEFORE this function is ever
 *     reached, per design D2's snapshot_ok formula) means: mark NOTHING
 *     as orphaned, present NOTHING as live, and return a status distinct
 *     from "the list is genuinely empty" so the caller can show an
 *     "unavailable" state — never a bare empty list, which a user reads
 *     as "you have no colours".
 *   - snapshot_ok == TRUE is the only case that may show or omit a row,
 *     and the join direction is: a row is shown ONLY for a stored entry
 *     whose xid is ALSO present in the snapshot. Everything else about
 *     removing an orphaned entry (the actual delete) stays exactly
 *     lc_winstore_reconcile()'s job, called unchanged by Phase 7 — this
 *     file never mutates an LcWinStore and never calls reconcile itself.
 *
 * Links glib only. Must NEVER include <gtk/gtk.h>, <xfconf/xfconf.h>, or
 * <libwnck/libwnck.h> — CI enforces this by scanning liblc-core.a for
 * undefined gtk_/xfconf_/wnck_ symbols. The live snapshot arrives here
 * purely as caller-supplied data (an array of {xid, title} pairs plus
 * the snapshot_ok flag), exactly the same shape discipline
 * lc_winstore_reconcile() already uses for its live_xids/n_live pair.
 */
#ifndef LC_WINLIST_H
#define LC_WINLIST_H

#include <glib.h>

G_BEGIN_DECLS

/* One window as reported by the caller's snapshot: an xid plus its
 * current title. `title` is NOT owned by this struct — the caller keeps
 * it valid for the duration of the lc_winlist_build() call that reads
 * it; lc_winlist_build() copies whatever it needs into the rows it
 * returns. */
typedef struct
{
  gulong xid;
  const gchar *title;
} LcWinlistWindow;

/* One row of the built live-colour list: a stored entry whose xid was
 * confirmed present in the snapshot, together with its current title.
 * `title` IS owned by this struct (a copy, never the caller's pointer)
 * and must be freed via lc_winlist_rows_free(). */
typedef struct
{
  gulong xid;
  gchar *title;
} LcWinlistRow;

/* LC_WINLIST_OK: the join ran against a positively-witnessed snapshot.
 * out_rows/out_n_rows are meaningful and may legitimately be zero rows
 * (every stored entry is genuinely orphaned against a real, non-empty
 * snapshot — a fact, not an ambiguity).
 *
 * LC_WINLIST_UNAVAILABLE: snapshot_ok was FALSE. *out_rows is always
 * empty in this case too, but the CALLER MUST NOT treat that the same
 * way as LC_WINLIST_OK with zero rows: this status means "cannot
 * determine," and the caller must render a distinct "window list
 * unavailable" state, not an empty colour list (see this file's header
 * comment and design D2). */
typedef enum
{
  LC_WINLIST_OK,
  LC_WINLIST_UNAVAILABLE
} LcWinlistStatus;

/* Pure join of `stored_xids` (as returned by lc_winstore_xids()) against
 * a caller-supplied snapshot. See this file's header comment for the
 * full fail-safe rationale; in short:
 *
 *   - snapshot_ok == FALSE ⇒ returns LC_WINLIST_UNAVAILABLE immediately,
 *     `*out_rows` set to NULL and `*out_n_rows` set to 0, WITHOUT
 *     reading `snapshot`/`n_snapshot` at all (an ambiguous snapshot is
 *     never trusted even defensively).
 *   - snapshot_ok == TRUE ⇒ returns LC_WINLIST_OK. For each entry in
 *     `stored_xids`, in order, a row is emitted iff a snapshot entry
 *     with the same xid exists; that row's title is a copy of the
 *     matching snapshot entry's title. A stored xid with no match is
 *     silently omitted (orphaned — but this function makes NO decision
 *     about deleting it; that stays lc_winstore_reconcile()'s job,
 *     called unchanged by the caller).
 *
 * `*out_rows` follows lc_winstore_xids()'s "*out_n, not the pointer"
 * contract on the OK path: it is never NULL when LC_WINLIST_OK is
 * returned, even for zero matching rows — callers must check
 * `*out_n_rows`, not the pointer, to detect an empty result.
 *
 * stored_xids may be NULL only when n_stored == 0. snapshot may be NULL
 * only when n_snapshot == 0. out_rows and out_n_rows must both be
 * non-NULL. Pure: performs no I/O, touches no LcWinStore, and calls no
 * libwnck function (this file links none). Caller owns the returned
 * rows (lc_winlist_rows_free()). */
LcWinlistStatus lc_winlist_build (const gulong *stored_xids, gsize n_stored,
                                   gboolean snapshot_ok,
                                   const LcWinlistWindow *snapshot, gsize n_snapshot,
                                   LcWinlistRow **out_rows, gsize *out_n_rows);

/* Frees rows and every title it owns. Safe to call with rows == NULL
 * (n_rows is then ignored). */
void lc_winlist_rows_free (LcWinlistRow *rows, gsize n_rows);

G_END_DECLS

#endif /* LC_WINLIST_H */
