/* tests/test-winlist.c — lc_winlist_build(): the pure live/orphan join
 * (design D2, D8), against the stored-color-management spec's two
 * headless-testable scenarios this module exists to close:
 *
 *   - "Orphaned entry never appears as a row" / "Row shows the current
 *     title" — exercised under a positively-witnessed (snapshot_ok ==
 *     TRUE) snapshot.
 *   - "List never marks entries as orphaned under an ambiguous
 *     snapshot" — exercised under snapshot_ok == FALSE, standing in for
 *     BOTH the unobtainable case and the obtained-but-empty case, since
 *     design D2's formula already collapses both into that single
 *     boolean before this function is ever reached.
 *
 * The fail-safe direction under test here is the SAME direction
 * lc_winstore_reconcile() already proved in tests/test-winstore.c: an
 * ambiguous snapshot must never be read as "nothing is open." This file
 * does not re-derive that rule independently — it pins the exact same
 * asymmetry at the join layer, which is the layer the stored-colour
 * management UI (Phase 7) actually reads from.
 */
#include <glib.h>
#include <string.h>
#include "lc-winlist.h"

#define XID_A 1001UL
#define XID_B 1002UL
#define XID_C 1003UL

/* --- snapshot_ok == TRUE ------------------------------------------- */

static void
test_winlist_orphaned_entry_never_appears_as_a_row (void)
{
  /* GIVEN stored entries for window A (currently open) and window B
   * (closed) AND a successfully-obtained, non-empty live snapshot
   * containing A but not B WHEN the list is built THEN it contains
   * exactly one row, for A, and no row exists for B. */
  gulong stored[] = { XID_A, XID_B };
  LcWinlistWindow snapshot[] = {
    { XID_A, "Terminal — A" },
  };
  LcWinlistRow *rows = NULL;
  gsize n_rows = 0;
  LcWinlistStatus status;

  status = lc_winlist_build (stored, G_N_ELEMENTS (stored),
                              TRUE, snapshot, G_N_ELEMENTS (snapshot),
                              &rows, &n_rows);

  g_assert_cmpint (status, ==, LC_WINLIST_OK);
  g_assert_nonnull (rows);
  g_assert_cmpuint (n_rows, ==, 1);
  g_assert_cmpuint (rows[0].xid, ==, XID_A);

  lc_winlist_rows_free (rows, n_rows);
}

static void
test_winlist_row_shows_the_current_title (void)
{
  /* GIVEN a stored entry whose XID matches an open window AND a
   * successfully-obtained, non-empty live snapshot WHEN the list is
   * built THEN the row's label is that window's current title, not a
   * cached or stale one. */
  gulong stored[] = { XID_A };
  LcWinlistWindow snapshot[] = {
    { XID_A, "Firefox — new tab" },
  };
  LcWinlistRow *rows = NULL;
  gsize n_rows = 0;
  LcWinlistStatus status;

  status = lc_winlist_build (stored, G_N_ELEMENTS (stored),
                              TRUE, snapshot, G_N_ELEMENTS (snapshot),
                              &rows, &n_rows);

  g_assert_cmpint (status, ==, LC_WINLIST_OK);
  g_assert_cmpuint (n_rows, ==, 1);
  g_assert_cmpstr (rows[0].title, ==, "Firefox — new tab");

  lc_winlist_rows_free (rows, n_rows);
}

static void
test_winlist_title_is_a_copy_not_the_caller_pointer (void)
{
  /* The row's title must survive the caller's snapshot buffer being
   * mutated or freed after lc_winlist_build() returns — otherwise a
   * caller that builds a transient snapshot array (exactly what Phase
   * 7's wnck glue will do) would hand back a dangling pointer. */
  gchar *title = g_strdup ("Editor — file.c");
  gulong stored[] = { XID_A };
  LcWinlistWindow snapshot[] = {
    { XID_A, title },
  };
  LcWinlistRow *rows = NULL;
  gsize n_rows = 0;

  lc_winlist_build (stored, G_N_ELEMENTS (stored),
                     TRUE, snapshot, G_N_ELEMENTS (snapshot),
                     &rows, &n_rows);

  g_assert_true (rows[0].title != title);

  memset (title, 'x', strlen (title));
  g_free (title);

  g_assert_cmpstr (rows[0].title, ==, "Editor — file.c");

  lc_winlist_rows_free (rows, n_rows);
}

static void
test_winlist_stored_order_is_preserved (void)
{
  /* Rows are emitted in stored_xids order, not snapshot order — matches
   * the project's existing "stable order" convention
   * (lc_winstore_xids(), design D7). */
  gulong stored[] = { XID_C, XID_A, XID_B };
  LcWinlistWindow snapshot[] = {
    { XID_A, "A" },
    { XID_B, "B" },
    { XID_C, "C" },
  };
  LcWinlistRow *rows = NULL;
  gsize n_rows = 0;

  lc_winlist_build (stored, G_N_ELEMENTS (stored),
                     TRUE, snapshot, G_N_ELEMENTS (snapshot),
                     &rows, &n_rows);

  g_assert_cmpuint (n_rows, ==, 3);
  g_assert_cmpuint (rows[0].xid, ==, XID_C);
  g_assert_cmpuint (rows[1].xid, ==, XID_A);
  g_assert_cmpuint (rows[2].xid, ==, XID_B);

  lc_winlist_rows_free (rows, n_rows);
}

static void
test_winlist_no_stored_entries_matches_snapshot_ok_true_is_empty_ok (void)
{
  /* An empty store under a genuinely valid snapshot is an ordinary,
   * non-ambiguous empty result: LC_WINLIST_OK with zero rows, NOT
   * LC_WINLIST_UNAVAILABLE — there is nothing ambiguous about "you
   * have never stored a colour." */
  LcWinlistWindow snapshot[] = {
    { XID_A, "A" },
  };
  LcWinlistRow *rows = NULL;
  gsize n_rows = 12345; /* poison, must be overwritten */
  LcWinlistStatus status;

  status = lc_winlist_build (NULL, 0,
                              TRUE, snapshot, G_N_ELEMENTS (snapshot),
                              &rows, &n_rows);

  g_assert_cmpint (status, ==, LC_WINLIST_OK);
  g_assert_cmpuint (n_rows, ==, 0);

  lc_winlist_rows_free (rows, n_rows);
}

/* --- snapshot_ok == FALSE -------------------------------------------
 *
 * Both branches below stand for the two production cases design D2
 * already collapses into a single snapshot_ok == FALSE before this
 * function is reached: "unobtainable" (no WnckScreen/display) and
 * "obtained-but-empty" (the measured libwnck async-population case: 0
 * windows at idle time, 29 windows 1.5 seconds later). This function
 * must not — and, per its contract, cannot — tell them apart, so a
 * single test exercising snapshot_ok == FALSE covers both by
 * construction; a second scenario below additionally passes a
 * non-empty `snapshot` array to confirm it is never even consulted.
 */

static void
test_winlist_ambiguous_snapshot_never_marks_entries_orphaned_or_live (void)
{
  /* GIVEN stored entries exist AND the live snapshot is either
   * unobtainable or obtained-but-empty WHEN the list view is built
   * THEN no stored entry is presented as orphaned, and the list shows
   * no rows as live either — it MUST NOT render every stored entry as
   * if orphaned. */
  gulong stored[] = { XID_A, XID_B, XID_C };
  LcWinlistRow *rows = (LcWinlistRow *) 0xdeadbeef; /* poison */
  gsize n_rows = 12345; /* poison */
  LcWinlistStatus status;

  status = lc_winlist_build (stored, G_N_ELEMENTS (stored),
                              FALSE, NULL, 0,
                              &rows, &n_rows);

  g_assert_cmpint (status, ==, LC_WINLIST_UNAVAILABLE);
  g_assert_null (rows);
  g_assert_cmpuint (n_rows, ==, 0);
}

static void
test_winlist_ambiguous_snapshot_ignores_a_non_empty_snapshot_argument (void)
{
  /* Defense in depth: even if a caller mistakenly hands over snapshot
   * data alongside snapshot_ok == FALSE (e.g. a stale buffer from a
   * previous, valid read), this function must not consult it — an
   * ambiguous snapshot is never trusted, not even partially. */
  gulong stored[] = { XID_A };
  LcWinlistWindow stale_snapshot[] = {
    { XID_A, "stale title from a previous valid read" },
  };
  LcWinlistRow *rows = NULL;
  gsize n_rows = 0;
  LcWinlistStatus status;

  status = lc_winlist_build (stored, G_N_ELEMENTS (stored),
                              FALSE, stale_snapshot, G_N_ELEMENTS (stale_snapshot),
                              &rows, &n_rows);

  g_assert_cmpint (status, ==, LC_WINLIST_UNAVAILABLE);
  g_assert_null (rows);
  g_assert_cmpuint (n_rows, ==, 0);
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/winlist/orphaned-entry-never-appears-as-a-row", test_winlist_orphaned_entry_never_appears_as_a_row);
  g_test_add_func ("/winlist/row-shows-the-current-title", test_winlist_row_shows_the_current_title);
  g_test_add_func ("/winlist/title-is-a-copy-not-the-caller-pointer", test_winlist_title_is_a_copy_not_the_caller_pointer);
  g_test_add_func ("/winlist/stored-order-is-preserved", test_winlist_stored_order_is_preserved);
  g_test_add_func ("/winlist/no-stored-entries-with-snapshot-ok-true-is-empty-ok", test_winlist_no_stored_entries_matches_snapshot_ok_true_is_empty_ok);
  g_test_add_func ("/winlist/ambiguous-snapshot-never-marks-entries-orphaned-or-live", test_winlist_ambiguous_snapshot_never_marks_entries_orphaned_or_live);
  g_test_add_func ("/winlist/ambiguous-snapshot-ignores-a-non-empty-snapshot-argument", test_winlist_ambiguous_snapshot_ignores_a_non_empty_snapshot_argument);

  return g_test_run ();
}
