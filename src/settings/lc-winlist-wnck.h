/* lc-winlist-wnck.h — the only wnck-touching part of the colour-list
 * feature (design.md D2/D3, D8): capturing a live window snapshot and
 * the self-XID completeness probe that decides whether that snapshot
 * may be trusted at all. The pure join and the snapshot_ok truth table
 * live in src/core/lc-winlist.{c,h} — this file only produces the
 * caller-supplied data that function's own header describes ("an
 * obtained flag plus an XID array"); it never calls lc_winlist_build()
 * itself, so lc-settings-ui.c stays the one place that wires wnck data
 * into the core join.
 *
 * WHY wnck_screen_force_update() IS CALLED HERE, AND WHY THAT IS SAFE
 * (design D3) — READ THIS BEFORE "FIXING" IT AS A COPY OF module.c's
 * BAN: src/glue/module.c's header documents incident 2, a libwnck
 * reentrancy CRITICAL from calling force_update() while the panel's own
 * tasklist plugin was already inside libwnck's update_client_list();
 * src/glue/lc-tasklist.c repeats the same warning at its own
 * (deliberately absent) force_update() call site. Neither ban applies
 * to this process: this is a separate, standalone GtkApplication binary
 * (design.md "Technical Approach") that never loads inside xfce4-panel
 * and contains no other libwnck client of its own to re-enter. It calls
 * gtk_init() itself (via GtkApplication) and only touches libwnck from
 * GtkWidget::map or later, so the main loop is already running and a
 * GdkDisplay is guaranteed — incident 1 (the crash-on-load from a NULL
 * display) cannot happen here either. If a wnck call does fault, it
 * takes down a reopenable settings dialog, not the panel (design D3's
 * own rationale). force_update() is called exactly once per refresh,
 * from exactly this file, and nowhere else in src/settings/.
 *
 * WHY THE SELF-XID PROBE, NOT JUST "n_live > 0" (design D2): lc-winstore.h
 * says explicitly that n_live > 0 cannot detect a PARTIAL snapshot, which
 * is exactly why src/glue/module.c refuses snapshot-based pruning
 * entirely (see its own header, incident 3). This process has one piece
 * of positive evidence the panel module could never have: once our own
 * toplevel window is mapped, it MUST appear in a complete
 * _NET_CLIENT_LIST. Its absence is proof the snapshot is still partial,
 * not merely a suggestion — so the probe is gated on the caller passing
 * an already-mapped `self_window` (see lc_winlist_wnck_capture()'s own
 * doc comment) and folds that evidence into snapshot_ok = (screen !=
 * NULL) && (n_live > 0) && (our_xid ∈ snapshot), matching D2's formula
 * exactly.
 *
 * Links gtk, glib and libwnck — never included from src/core/.
 */
#ifndef LC_WINLIST_WNCK_H
#define LC_WINLIST_WNCK_H

#include <gtk/gtk.h>

#include "lc-winlist.h"

G_BEGIN_DECLS

/* One freshly captured snapshot: whether it may be trusted at all
 * (design D2's snapshot_ok formula, already collapsed), and — only when
 * trusted — every currently open window's {xid, title} pair.
 *
 * `windows` is always NULL and `n_windows` is always 0 when
 * `snapshot_ok` is FALSE: there is nothing usable to read, and
 * lc_winlist_wnck_capture() does not keep an untrusted snapshot around
 * at all. When `snapshot_ok` is TRUE, `windows` is never NULL, even for
 * a hypothetical zero-window result — but note that case cannot actually
 * happen here, because `n_live > 0` is itself part of the snapshot_ok
 * formula: a trusted snapshot always has at least one window (our own).
 *
 * Every `windows[i].title` is a BORROWED pointer into libwnck's own
 * window-name storage — not owned by this struct, exactly matching
 * LcWinlistWindow's documented contract in lc-winlist.h. It stays valid
 * only until the next libwnck call for that window (a title change, the
 * window closing, etc.), so a caller MUST finish using it — typically by
 * handing the whole snapshot to lc_winlist_build(), which copies
 * whatever it needs into its own owned rows — before doing anything else
 * that could re-enter libwnck. Caller owns the snapshot struct itself
 * (lc_winlist_wnck_snapshot_clear()). */
typedef struct
{
  gboolean snapshot_ok;
  LcWinlistWindow *windows;
  gsize n_windows;
} LcWinlistWnckSnapshot;

/* Captures a fresh snapshot for the colour-list view (design D2/D3):
 * wnck_screen_force_update() once, then the self-XID completeness probe
 * against `self_window`'s own GdkWindow.
 *
 * `self_window` MUST already be realized and mapped — call this only
 * from a GtkWidget::map handler or later (never from window
 * construction), or our own XID cannot yet be in any snapshot and the
 * probe would always fail. Passing an unrealized widget is a programming
 * error (g_return_val_if_fail) and returns an unavailable snapshot
 * without touching libwnck at all.
 *
 * Returns a snapshot with `snapshot_ok == FALSE` (and `windows == NULL`)
 * if no WnckScreen is available, the live list comes back empty, or our
 * own XID is absent from a non-empty one — the exact same "cannot
 * determine, prune nothing" case lc_winlist_build() and
 * lc_winstore_reconcile() both already treat identically to an
 * unobtainable snapshot. */
LcWinlistWnckSnapshot lc_winlist_wnck_capture (GtkWidget *self_window);

/* Newly allocated array of every xid in `snapshot->windows`, in the same
 * order — the exact shape lc_winstore_reconcile()'s `live_xids`/`n_live`
 * parameters need, so a caller doing bulk cleanup never has to read
 * `windows[i].title` at all. Sets *out_n. Returns NULL with *out_n == 0
 * for an untrusted (`snapshot_ok == FALSE`) or empty snapshot.
 * `snapshot` may be NULL (treated as untrusted); out_n must be non-NULL.
 * Caller owns the returned array (g_free). */
gulong *lc_winlist_wnck_snapshot_xids (const LcWinlistWnckSnapshot *snapshot, gsize *out_n);

/* Frees `snapshot->windows` (never the borrowed titles inside it — see
 * this struct's own doc comment) and resets the struct to the
 * `snapshot_ok == FALSE` / empty state. Safe to call on an
 * already-untrusted or already-cleared snapshot, and safe to call
 * twice. `snapshot` may be NULL. */
void lc_winlist_wnck_snapshot_clear (LcWinlistWnckSnapshot *snapshot);

G_END_DECLS

#endif /* LC_WINLIST_WNCK_H */
