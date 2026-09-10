/* lc-winlist.c — see lc-winlist.h */
#include "lc-winlist.h"

LcWinlistStatus
lc_winlist_build (const gulong *stored_xids, gsize n_stored,
                   gboolean snapshot_ok,
                   const LcWinlistWindow *snapshot, gsize n_snapshot,
                   LcWinlistRow **out_rows, gsize *out_n_rows)
{
  LcWinlistRow *rows;
  gsize n_rows = 0;
  gsize i;

  g_return_val_if_fail (out_rows != NULL, LC_WINLIST_UNAVAILABLE);
  g_return_val_if_fail (out_n_rows != NULL, LC_WINLIST_UNAVAILABLE);
  g_return_val_if_fail (n_stored == 0 || stored_xids != NULL, LC_WINLIST_UNAVAILABLE);
  g_return_val_if_fail (n_snapshot == 0 || snapshot != NULL, LC_WINLIST_UNAVAILABLE);

  *out_rows = NULL;
  *out_n_rows = 0;

  /* Fail-safe direction, mirrored exactly from lc_winstore_reconcile():
   * an ambiguous snapshot_ok == FALSE (the wnck-facing caller has
   * already collapsed BOTH "unobtainable" and "obtained-but-empty" into
   * this single boolean, per design D2) must never let a single stored
   * entry be judged orphaned or live. `snapshot`/`n_snapshot` are not
   * even read on this path — an ambiguous snapshot is never trusted,
   * not even defensively. Returning LC_WINLIST_UNAVAILABLE (rather than
   * LC_WINLIST_OK with zero rows) is the whole point: it gives the
   * caller a status distinct from "genuinely no live windows," so a
   * "window list unavailable" state can be shown instead of a bare
   * empty colour list a user would read as "you have no colours." */
  if (!snapshot_ok)
    return LC_WINLIST_UNAVAILABLE;

  /* Never NULL on this path, mirroring lc_winstore_xids()'s "*out_n,
   * not the pointer" contract: callers must check *out_n_rows, not the
   * returned pointer, to detect an empty (but non-ambiguous) result. */
  rows = g_new0 (LcWinlistRow, n_stored > 0 ? n_stored : 1);

  /* Join direction: walk stored_xids in order (preserving the same
   * stable order lc_winstore_xids() already guarantees, design D7) and
   * emit a row only for a stored xid that is ALSO present in the
   * snapshot. A stored xid with no match is silently omitted here —
   * this function makes no decision about deleting it; that stays
   * lc_winstore_reconcile()'s job alone, called unchanged by the
   * caller (Phase 7). Nested-loop shape matches
   * lc_winstore_reconcile()'s own live-set lookup for the same reason:
   * this is the join layer for the exact hazard that function already
   * proved safe, not a differently-shaped rewrite of it. */
  for (i = 0; i < n_stored; i++)
    {
      gulong xid = stored_xids[i];
      gsize j;

      for (j = 0; j < n_snapshot; j++)
        {
          if (snapshot[j].xid == xid)
            {
              rows[n_rows].xid = xid;
              rows[n_rows].title = g_strdup (snapshot[j].title != NULL ? snapshot[j].title : "");
              n_rows++;
              break;
            }
        }
    }

  *out_rows = rows;
  *out_n_rows = n_rows;

  return LC_WINLIST_OK;
}

void
lc_winlist_rows_free (LcWinlistRow *rows, gsize n_rows)
{
  gsize i;

  if (rows == NULL)
    return;

  for (i = 0; i < n_rows; i++)
    g_free (rows[i].title);

  g_free (rows);
}
