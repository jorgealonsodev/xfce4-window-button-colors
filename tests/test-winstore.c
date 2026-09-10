/* tests/test-winstore.c — LcWinStore load/save/set/unset/has/get/size,
 * lc_winstore_reconcile()'s fail-safe truth table, and
 * lc_winstore_render()'s document shape.
 *
 * Persistence-degradation coverage mirrors the launcher-era
 * test-store-load.c's discipline (banner is never a validity gate, at
 * most one warning per load, a malformed record is skipped whole without
 * disturbing its siblings). lc_winstore_reconcile()'s truth table is the
 * one piece of this file that must NOT be modeled on the launcher era:
 * see the fail-safe-direction cases below, which assert the OPPOSITE of
 * what an id+fingerprint reconciler would do.
 */
#include <glib.h>
#include <glib/gstdio.h>
#include <locale.h>
#include <string.h>
#include "lc-winstore.h"
#include "lc-document.h"
#include "lc-color.h"
#include "lc-winid.h"

#define SAMPLE_XID_A 138412036UL
#define SAMPLE_XID_B 138412099UL
#define SAMPLE_XID_C 999UL

static gchar *
write_temp_css (const gchar *content)
{
  GError *error = NULL;
  gchar *path = NULL;
  gint fd;

  fd = g_file_open_tmp ("lc-winstore-test-XXXXXX.css", &path, &error);
  g_assert_no_error (error);
  g_assert_cmpint (fd, >=, 0);
  g_close (fd, NULL);

  g_assert_true (g_file_set_contents (path, content, -1, &error));
  g_assert_no_error (error);

  return path;
}

/* ---- basic CRUD ---------------------------------------------------- */

static void
test_winstore_new_is_empty (void)
{
  LcWinStore *store = lc_winstore_new ();

  g_assert_nonnull (store);
  g_assert_cmpuint (lc_winstore_size (store), ==, 0);
  g_assert_false (lc_winstore_has (store, SAMPLE_XID_A));

  lc_winstore_free (store);
}

static void
test_winstore_set_then_has_and_get (void)
{
  LcWinStore *store = lc_winstore_new ();
  LcColor color = { 0xE5, 0x39, 0x35, 0xD9 };
  LcColor out = { 0, 0, 0, 0 };

  lc_winstore_set (store, SAMPLE_XID_A, &color);

  g_assert_cmpuint (lc_winstore_size (store), ==, 1);
  g_assert_true (lc_winstore_has (store, SAMPLE_XID_A));
  g_assert_true (lc_winstore_get (store, SAMPLE_XID_A, &out));
  g_assert_cmpuint (out.r, ==, color.r);
  g_assert_cmpuint (out.g, ==, color.g);
  g_assert_cmpuint (out.b, ==, color.b);
  g_assert_cmpuint (out.a, ==, color.a);

  lc_winstore_free (store);
}

static void
test_winstore_set_overwrites_existing_entry (void)
{
  LcWinStore *store = lc_winstore_new ();
  LcColor first = { 0x10, 0x10, 0x10, 0xFF };
  LcColor second = { 0x20, 0x20, 0x20, 0x80 };
  LcColor out = { 0, 0, 0, 0 };

  lc_winstore_set (store, SAMPLE_XID_A, &first);
  lc_winstore_set (store, SAMPLE_XID_A, &second);

  g_assert_cmpuint (lc_winstore_size (store), ==, 1);
  g_assert_true (lc_winstore_get (store, SAMPLE_XID_A, &out));
  g_assert_cmpuint (out.r, ==, second.r);

  lc_winstore_free (store);
}

static void
test_winstore_unset_removes_entry (void)
{
  LcWinStore *store = lc_winstore_new ();
  LcColor color = { 1, 2, 3, 4 };

  lc_winstore_set (store, SAMPLE_XID_A, &color);
  g_assert_true (lc_winstore_unset (store, SAMPLE_XID_A));

  g_assert_cmpuint (lc_winstore_size (store), ==, 0);
  g_assert_false (lc_winstore_has (store, SAMPLE_XID_A));

  lc_winstore_free (store);
}

static void
test_winstore_unset_missing_entry_returns_false (void)
{
  LcWinStore *store = lc_winstore_new ();

  g_assert_false (lc_winstore_unset (store, SAMPLE_XID_A));

  lc_winstore_free (store);
}

static void
test_winstore_get_missing_entry_returns_false_and_leaves_out_untouched (void)
{
  LcWinStore *store = lc_winstore_new ();
  LcColor out = { 0xAA, 0xBB, 0xCC, 0xDD };

  g_assert_false (lc_winstore_get (store, SAMPLE_XID_A, &out));
  g_assert_cmpuint (out.r, ==, 0xAA);

  lc_winstore_free (store);
}

static void
test_winstore_set_ignores_invalid_xid (void)
{
  LcWinStore *store = lc_winstore_new ();
  LcColor color = { 1, 2, 3, 4 };

  lc_winstore_set (store, 0, &color); /* 0 == X11 "None", never valid */

  g_assert_cmpuint (lc_winstore_size (store), ==, 0);

  lc_winstore_free (store);
}

/* ---- load: degrade-safely paths ------------------------------------ */

static void
test_winstore_load_missing_file_degrades_to_empty_silently (void)
{
  /* A missing file is the ordinary first-run case (nothing has ever been
   * saved yet), not a malformed one: it must NOT emit a g_warning — a
   * normal first panel start has to stay quiet. This is a deliberate
   * behaviour change: see module.c's crash-on-load fix, where a noisy
   * first-run warning was part of the same investigation (not the
   * cause, but tidied up alongside it). No g_test_expect_message() here
   * is itself the assertion: an unexpected WARNING/CRITICAL would fail
   * the test process by default. */
  GError *error = NULL;
  gchar *path = NULL;
  LcWinStore *store;
  gint fd = g_file_open_tmp ("lc-winstore-missing-XXXXXX.css", &path, &error);

  g_assert_no_error (error);
  g_close (fd, NULL);
  g_assert_cmpint (g_remove (path), ==, 0);

  store = lc_winstore_load (path);

  g_assert_nonnull (store);
  g_assert_cmpuint (lc_winstore_size (store), ==, 0);

  lc_winstore_free (store);
  g_free (path);
}

static void
test_winstore_load_unreadable_file_degrades_to_empty_with_one_warning (void)
{
  gchar *path = write_temp_css (
    "/* xfce4-window-button-colors — generated file (window colours), do not edit by hand. schema=1 */\n");
  LcWinStore *store;

  g_assert_cmpint (g_chmod (path, 0000), ==, 0);

  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "*lc_winstore_load*");
  store = lc_winstore_load (path);
  g_test_assert_expected_messages ();

  g_assert_nonnull (store);
  g_assert_cmpuint (lc_winstore_size (store), ==, 0);

  lc_winstore_free (store);
  g_chmod (path, 0644);
  g_remove (path);
  g_free (path);
}

static void
test_winstore_load_malformed_content_degrades_to_empty_with_one_warning (void)
{
  gchar *path = write_temp_css (
    "this is not a colors.css file at all\n"
    "{ unterminated rule ;;; @import \"nope\";\n");
  LcWinStore *store;

  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "*lc_winstore_load*");
  store = lc_winstore_load (path);
  g_test_assert_expected_messages ();

  g_assert_nonnull (store);
  g_assert_cmpuint (lc_winstore_size (store), ==, 0);

  lc_winstore_free (store);
  g_remove (path);
  g_free (path);
}

static void
test_winstore_load_null_path_degrades_to_empty_with_one_warning (void)
{
  LcWinStore *store;

  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "*lc_winstore_load*");
  store = lc_winstore_load (NULL);
  g_test_assert_expected_messages ();

  g_assert_nonnull (store);
  g_assert_cmpuint (lc_winstore_size (store), ==, 0);

  lc_winstore_free (store);
}

/* ---- save/load round trip ------------------------------------------- */

static void
test_winstore_save_then_load_round_trips (void)
{
  gchar *path;
  LcWinStore *store = lc_winstore_new ();
  LcWinStore *reloaded;
  LcColor color_a = { 0xE5, 0x39, 0x35, 0xD9 };
  LcColor color_b = { 0x1E, 0x88, 0xE5, 0x80 };
  LcColor out = { 0, 0, 0, 0 };
  GError *error = NULL;
  gint fd = g_file_open_tmp ("lc-winstore-roundtrip-XXXXXX.css", &path, &error);

  g_assert_no_error (error);
  g_close (fd, NULL);

  lc_winstore_set (store, SAMPLE_XID_A, &color_a);
  lc_winstore_set (store, SAMPLE_XID_B, &color_b);

  g_assert_true (lc_winstore_save (store, path));

  reloaded = lc_winstore_load (path);
  g_assert_cmpuint (lc_winstore_size (reloaded), ==, 2);

  g_assert_true (lc_winstore_get (reloaded, SAMPLE_XID_A, &out));
  g_assert_cmpuint (out.r, ==, color_a.r);
  g_assert_cmpuint (out.a, ==, color_a.a);

  g_assert_true (lc_winstore_get (reloaded, SAMPLE_XID_B, &out));
  g_assert_cmpuint (out.b, ==, color_b.b);

  lc_winstore_free (store);
  lc_winstore_free (reloaded);
  g_remove (path);
  g_free (path);
}

/* Regression guard for the real-world fresh-install failure discovered
 * while verifying the crash-on-load fix: on a genuinely fresh install
 * ~/.config/xfce4-window-button-colors/ does not exist yet, and
 * g_file_set_contents() alone cannot create a missing parent directory.
 * Without lc_winstore_save() creating it first, no colour a first-time
 * user picks would ever actually persist. */
static void
test_winstore_save_creates_missing_parent_directory (void)
{
  gchar *base_dir = g_dir_make_tmp ("lc-winstore-save-parent-XXXXXX", NULL);
  gchar *nested_dir = g_build_filename (base_dir, "does", "not", "exist", "yet", NULL);
  gchar *path = g_build_filename (nested_dir, "colors.css", NULL);
  LcWinStore *store = lc_winstore_new ();
  LcColor color = { 30, 136, 229, 217 };
  LcWinStore *reloaded;

  g_assert_nonnull (base_dir);
  g_assert_false (g_file_test (nested_dir, G_FILE_TEST_EXISTS));

  lc_winstore_set (store, SAMPLE_XID_A, &color);

  g_assert_true (lc_winstore_save (store, path));
  g_assert_true (g_file_test (path, G_FILE_TEST_EXISTS));

  reloaded = lc_winstore_load (path);
  g_assert_cmpuint (lc_winstore_size (reloaded), ==, 1);

  lc_winstore_free (store);
  lc_winstore_free (reloaded);
  g_remove (path);
  g_rmdir (nested_dir);
  {
    gchar *p1 = g_path_get_dirname (nested_dir);
    gchar *p2 = g_path_get_dirname (p1);
    gchar *p3 = g_path_get_dirname (p2);
    g_rmdir (p1);
    g_rmdir (p2);
    g_rmdir (p3);
    g_free (p1);
    g_free (p2);
    g_free (p3);
  }
  g_free (path);
  g_free (nested_dir);
  g_free (base_dir);
}

static void
test_winstore_entries_survive_a_missing_banner (void)
{
  /* Carried over from the launcher-era lesson: the banner is a
   * format-migration marker, never a validity gate. A hand-edited file
   * that lost its first line must not cost the user their colours. */
  gchar *content = g_strdup_printf (
    "/* xid=%lu */\n"
    ".lc-win-%lu {\n"
    "  background-image: none;\n"
    "  background-color: rgba(229,57,53,0.85);\n"
    "  border-radius: 4px;\n"
    "  margin: 0px;\n"
    "}\n",
    SAMPLE_XID_A, SAMPLE_XID_A);
  gchar *path = write_temp_css (content);
  LcWinStore *store;

  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "*lc_winstore_load*");
  store = lc_winstore_load (path);
  g_test_assert_expected_messages ();

  g_assert_cmpuint (lc_winstore_size (store), ==, 1);
  g_assert_true (lc_winstore_has (store, SAMPLE_XID_A));

  lc_winstore_free (store);
  g_remove (path);
  g_free (path);
  g_free (content);
}

static void
test_winstore_record_with_mismatched_selector_is_skipped_whole (void)
{
  gchar *good_color = "rgba(30,136,229,0.50)";
  gchar *content = g_strdup_printf (
    "/* xfce4-window-button-colors — generated file (window colours), do not edit by hand. schema=1 */\n"
    "\n"
    "/* xid=%lu */\n"
    ".lc-win-999999999 {\n" /* marker claims SAMPLE_XID_A, rule targets a different xid */
    "  background-image: none;\n"
    "  background-color: rgba(1,2,3,0.10);\n"
    "  border-radius: 4px;\n"
    "  margin: 0px;\n"
    "}\n"
    "\n"
    "/* xid=%lu */\n"
    ".lc-win-%lu {\n"
    "  background-image: none;\n"
    "  background-color: %s;\n"
    "  border-radius: 4px;\n"
    "  margin: 0px;\n"
    "}\n",
    SAMPLE_XID_A, SAMPLE_XID_B, SAMPLE_XID_B, good_color);
  gchar *path = write_temp_css (content);
  LcWinStore *store = lc_winstore_load (path);

  g_assert_cmpuint (lc_winstore_size (store), ==, 1);
  g_assert_false (lc_winstore_has (store, SAMPLE_XID_A));
  g_assert_true (lc_winstore_has (store, SAMPLE_XID_B));

  lc_winstore_free (store);
  g_remove (path);
  g_free (path);
  g_free (content);
}

static void
test_winstore_state_rule_with_extra_declaration_is_skipped_whole (void)
{
  gchar *content = g_strdup_printf (
    "/* xfce4-window-button-colors — generated file (window colours), do not edit by hand. schema=1 */\n"
    "\n"
    "/* xid=%lu */\n"
    ".lc-win-%lu {\n"
    "  background-image: none;\n"
    "  background-color: rgba(10,20,30,0.90);\n"
    "  border-radius: 4px;\n"
    "  margin: 0px;\n"
    "}\n"
    ".lc-win-%lu:hover {\n"
    "  background-color: rgba(20,30,40,0.60);\n"
    "  border-radius: 4px;\n" /* extra declaration: must invalidate the whole record */
    "}\n"
    "\n"
    "/* xid=%lu */\n"
    ".lc-win-%lu {\n"
    "  background-image: none;\n"
    "  background-color: rgba(30,136,229,0.50);\n"
    "  border-radius: 4px;\n"
    "  margin: 0px;\n"
    "}\n",
    SAMPLE_XID_A, SAMPLE_XID_A, SAMPLE_XID_A, SAMPLE_XID_B, SAMPLE_XID_B);
  gchar *path = write_temp_css (content);
  LcWinStore *store;

  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "*lc_winstore_load*");
  store = lc_winstore_load (path);
  g_test_assert_expected_messages ();

  g_assert_cmpuint (lc_winstore_size (store), ==, 1);
  g_assert_false (lc_winstore_has (store, SAMPLE_XID_A));
  g_assert_true (lc_winstore_has (store, SAMPLE_XID_B));

  lc_winstore_free (store);
  g_remove (path);
  g_free (path);
  g_free (content);
}

/* ---- render ----------------------------------------------------------*/

static void
test_winstore_render_reproduces_marker_class_and_declarations (void)
{
  LcWinStore *store = lc_winstore_new ();
  LcColor color = { 229, 57, 53, 217 }; /* 0.85 alpha, rounds like lc_color_to_css */
  LcDocument *doc;
  const gchar *css;
  gchar *expected_marker;
  gchar *expected_class;

  lc_winstore_set (store, SAMPLE_XID_A, &color);

  doc = lc_winstore_render (store);
  g_assert_nonnull (doc);
  css = lc_document_css (doc);
  g_assert_nonnull (css);

  expected_marker = g_strdup_printf ("xid=%lu", SAMPLE_XID_A);
  expected_class = lc_winid_css_class (SAMPLE_XID_A);

  g_assert_nonnull (strstr (css, expected_marker));
  g_assert_nonnull (strstr (css, expected_class));
  g_assert_nonnull (strstr (css, "background-color: rgba(229,57,53,0.85)"));
  g_assert_nonnull (strstr (css, ":hover"));
  g_assert_nonnull (strstr (css, ":active"));

  g_free (expected_marker);
  g_free (expected_class);
  lc_document_free (doc);
  lc_winstore_free (store);
}

static void
test_winstore_render_of_empty_store_has_no_entries (void)
{
  LcWinStore *store = lc_winstore_new ();
  LcDocument *doc = lc_winstore_render (store);
  const gchar *css = lc_document_css (doc);

  g_assert_nonnull (css);
  g_assert_null (strstr (css, "xid="));
  g_assert_null (strstr (css, "lc-win-"));

  lc_document_free (doc);
  lc_winstore_free (store);
}

/* PROJECT RULE regression: every number emitted into CSS must go through
 * g_ascii_formatd, never printf("%f"). A comma-decimal LC_NUMERIC must
 * not corrupt the rendered alpha. */
static void
test_winstore_render_alpha_is_locale_independent (void)
{
  static const gchar *comma_locales[] = { "es_ES.UTF-8", "es_ES", "de_DE.UTF-8", "de_DE", NULL };
  gchar *saved = g_strdup (setlocale (LC_NUMERIC, NULL));
  gchar *applied = NULL;
  LcWinStore *store;
  LcColor color = { 30, 136, 229, 217 }; /* alpha 0.85 */
  LcDocument *doc;
  const gchar *css;
  gsize i;

  for (i = 0; comma_locales[i] != NULL && applied == NULL; i++)
    applied = setlocale (LC_NUMERIC, comma_locales[i]);

  store = lc_winstore_new ();
  lc_winstore_set (store, SAMPLE_XID_A, &color);
  doc = lc_winstore_render (store);
  css = lc_document_css (doc);

  g_assert_nonnull (strstr (css, "0.85"));
  g_assert_null (strstr (css, "0,85"));

  if (applied == NULL)
    g_test_message ("no comma-decimal locale installed; assertion still held under C");

  setlocale (LC_NUMERIC, saved);
  g_free (saved);

  lc_document_free (doc);
  lc_winstore_free (store);
}

/* ---- xids: enumeration (D7) ------------------------------------------
 *
 * lc_winstore_xids() is the accessor the colour-list view (Phase 6/7)
 * needs and lc-winstore.h did not previously expose: `size`, `has`, and
 * `get(xid)` cannot enumerate. These tests pin its three contractual
 * properties: never NULL, empty-store shape, stable order, and that its
 * output type is directly usable as lc_winstore_reconcile()'s
 * live-snapshot input without any conversion in between.
 */

static void
test_winstore_xids_of_empty_store_returns_empty_array (void)
{
  LcWinStore *store = lc_winstore_new ();
  gsize out_n = 12345; /* sentinel: proves the function actually wrote it */
  gulong *xids = lc_winstore_xids (store, &out_n);

  g_assert_nonnull (xids); /* never NULL, even for an empty store */
  g_assert_cmpuint (out_n, ==, 0);

  g_free (xids);
  lc_winstore_free (store);
}

static void
test_winstore_xids_returns_stable_insertion_order (void)
{
  LcWinStore *store = lc_winstore_new ();
  LcColor color = { 1, 2, 3, 4 };
  gsize out_n = 0;
  gulong *xids;

  /* Inserted out of numeric order on purpose: a correct implementation
   * must reproduce insertion order, not sort or otherwise reorder. */
  lc_winstore_set (store, SAMPLE_XID_B, &color);
  lc_winstore_set (store, SAMPLE_XID_A, &color);
  lc_winstore_set (store, SAMPLE_XID_C, &color);

  xids = lc_winstore_xids (store, &out_n);

  g_assert_nonnull (xids);
  g_assert_cmpuint (out_n, ==, 3);
  g_assert_cmpuint (xids[0], ==, SAMPLE_XID_B);
  g_assert_cmpuint (xids[1], ==, SAMPLE_XID_A);
  g_assert_cmpuint (xids[2], ==, SAMPLE_XID_C);

  g_free (xids);
  lc_winstore_free (store);
}

static void
test_winstore_xids_feeds_reconcile_unchanged (void)
{
  /* The whole point of this accessor's signature (gulong * / gsize, the
   * same shape lc_winstore_reconcile() already takes) is that its
   * output can be handed straight to reconcile() with no conversion.
   * Enumerate every stored xid, then reconcile the SAME store against
   * that exact snapshot: since every entry is, by construction, present
   * in its own enumeration, nothing may be dropped. */
  LcWinStore *store = lc_winstore_new ();
  LcColor color = { 5, 6, 7, 8 };
  gsize n = 0;
  gulong *xids;
  guint pruned;

  lc_winstore_set (store, SAMPLE_XID_A, &color);
  lc_winstore_set (store, SAMPLE_XID_B, &color);

  xids = lc_winstore_xids (store, &n);
  pruned = lc_winstore_reconcile (store, TRUE, xids, n);

  g_assert_cmpuint (pruned, ==, 0);
  g_assert_cmpuint (lc_winstore_size (store), ==, 2);
  g_assert_true (lc_winstore_has (store, SAMPLE_XID_A));
  g_assert_true (lc_winstore_has (store, SAMPLE_XID_B));

  g_free (xids);
  lc_winstore_free (store);
}

/* ---- reconcile: the fail-safe truth table --------------------------- */

static void
test_winstore_reconcile_keeps_everything_when_live_list_not_obtained (void)
{
  LcWinStore *store = lc_winstore_new ();
  LcColor color = { 1, 2, 3, 4 };
  guint pruned;

  lc_winstore_set (store, SAMPLE_XID_A, &color);
  lc_winstore_set (store, SAMPLE_XID_B, &color);

  /* live_list_obtained == FALSE: an inability to check must never be
   * treated as absence, no matter what (even NULL/0) is passed for the
   * snapshot itself. */
  pruned = lc_winstore_reconcile (store, FALSE, NULL, 0);

  g_assert_cmpuint (pruned, ==, 0);
  g_assert_cmpuint (lc_winstore_size (store), ==, 2);
  g_assert_true (lc_winstore_has (store, SAMPLE_XID_A));
  g_assert_true (lc_winstore_has (store, SAMPLE_XID_B));

  lc_winstore_free (store);
}

static void
test_winstore_reconcile_drops_entries_confirmed_absent (void)
{
  LcWinStore *store = lc_winstore_new ();
  LcColor color = { 1, 2, 3, 4 };
  gulong live[] = { SAMPLE_XID_B };
  guint pruned;

  lc_winstore_set (store, SAMPLE_XID_A, &color);
  lc_winstore_set (store, SAMPLE_XID_B, &color);

  pruned = lc_winstore_reconcile (store, TRUE, live, G_N_ELEMENTS (live));

  g_assert_cmpuint (pruned, ==, 1);
  g_assert_cmpuint (lc_winstore_size (store), ==, 1);
  g_assert_false (lc_winstore_has (store, SAMPLE_XID_A));
  g_assert_true (lc_winstore_has (store, SAMPLE_XID_B));

  lc_winstore_free (store);
}

static void
test_winstore_reconcile_keeps_entries_present_in_live_list (void)
{
  LcWinStore *store = lc_winstore_new ();
  LcColor color = { 1, 2, 3, 4 };
  gulong live[] = { SAMPLE_XID_A, SAMPLE_XID_B, SAMPLE_XID_C };
  guint pruned;

  lc_winstore_set (store, SAMPLE_XID_A, &color);
  lc_winstore_set (store, SAMPLE_XID_B, &color);

  pruned = lc_winstore_reconcile (store, TRUE, live, G_N_ELEMENTS (live));

  g_assert_cmpuint (pruned, ==, 0);
  g_assert_cmpuint (lc_winstore_size (store), ==, 2);

  lc_winstore_free (store);
}

static void
test_winstore_reconcile_with_successfully_obtained_empty_list_keeps_everything (void)
{
  /* THE RULE (fixed after a real, measured production data-loss bug): an
   * empty-but-successfully-obtained live list is indistinguishable from
   * a snapshot taken before libwnck has finished populating its window
   * list asynchronously, and must NEVER authorise dropping anything.
   * Measured on a live panel: 0 windows at the moment this ran, 29
   * windows 1.5s later. The earlier version of this test asserted the
   * opposite ("drops all") and was itself the bug: it encoded the
   * destructive interpretation of an ambiguity this function has no way
   * to resolve, and a green suite shipped exactly the failure it should
   * have caught. `n_live == 0` now gets identical treatment to
   * `live_list_obtained == FALSE` — keep everything, prune nothing,
   * regardless of which of the two made the list empty. */
  LcWinStore *store = lc_winstore_new ();
  LcColor color = { 1, 2, 3, 4 };
  guint pruned;

  lc_winstore_set (store, SAMPLE_XID_A, &color);
  lc_winstore_set (store, SAMPLE_XID_B, &color);

  pruned = lc_winstore_reconcile (store, TRUE, NULL, 0);

  g_assert_cmpuint (pruned, ==, 0);
  g_assert_cmpuint (lc_winstore_size (store), ==, 2);
  g_assert_true (lc_winstore_has (store, SAMPLE_XID_A));
  g_assert_true (lc_winstore_has (store, SAMPLE_XID_B));

  lc_winstore_free (store);
}

/* Reproduces the actual production failure end-to-end at the persistence
 * layer: a store loaded from disk with real entries, reconciled against
 * a not-ready (empty-but-obtained) live snapshot exactly as
 * gtk_module_init()'s old deferred-activation path did, must not lose
 * anything AND must not rewrite the file — the two-part rule this bug
 * needed both halves of. A caller that (correctly) only calls
 * lc_winstore_save() when lc_winstore_reconcile() reports a nonzero
 * drop count, as the shipped glue now does, leaves the file byte-for-byte
 * untouched here. */
static void
test_winstore_reconcile_with_notready_snapshot_does_not_authorize_a_write (void)
{
  gchar *original_content;
  gsize original_length;
  gchar *reread_content;
  gsize reread_length;
  GError *error = NULL;
  LcWinStore *store;
  guint dropped;
  gchar *path = write_temp_css (
    "/* xfce4-window-button-colors — generated file (window colours), do not edit by hand. schema=1 */\n"
    "\n"
    "/* xid=138412036 */\n"
    ".lc-win-138412036 {\n"
    "  background-image: none;\n"
    "  background-color: rgba(229,57,53,0.85);\n"
    "  border-radius: 4px;\n"
    "  margin: 0px;\n"
    "}\n");

  g_assert_true (g_file_get_contents (path, &original_content, &original_length, &error));
  g_assert_no_error (error);

  store = lc_winstore_load (path);
  g_assert_cmpuint (lc_winstore_size (store), ==, 1);

  /* The not-ready snapshot: successfully obtained, genuinely empty. */
  dropped = lc_winstore_reconcile (store, TRUE, NULL, 0);
  g_assert_cmpuint (dropped, ==, 0);
  g_assert_cmpuint (lc_winstore_size (store), ==, 1);
  g_assert_true (lc_winstore_has (store, SAMPLE_XID_A));

  /* THE POLICY under test: a caller only ever calls lc_winstore_save()
   * when reconcile actually dropped something. dropped == 0 here, so no
   * save happens — simulating exactly what the fixed glue layer does. */
  if (dropped > 0)
    g_assert_true (lc_winstore_save (store, path));

  g_assert_true (g_file_get_contents (path, &reread_content, &reread_length, &error));
  g_assert_no_error (error);
  g_assert_cmpuint (reread_length, ==, original_length);
  g_assert_cmpstr (reread_content, ==, original_content);

  g_free (original_content);
  g_free (reread_content);
  lc_winstore_free (store);
  g_remove (path);
  g_free (path);
}

static void
test_winstore_reconcile_returns_zero_on_empty_store (void)
{
  LcWinStore *store = lc_winstore_new ();
  gulong live[] = { SAMPLE_XID_A };

  g_assert_cmpuint (lc_winstore_reconcile (store, TRUE, live, G_N_ELEMENTS (live)), ==, 0);
  g_assert_cmpuint (lc_winstore_reconcile (store, FALSE, NULL, 0), ==, 0);

  lc_winstore_free (store);
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/winstore/new-is-empty", test_winstore_new_is_empty);
  g_test_add_func ("/winstore/set-then-has-and-get", test_winstore_set_then_has_and_get);
  g_test_add_func ("/winstore/set-overwrites-existing-entry", test_winstore_set_overwrites_existing_entry);
  g_test_add_func ("/winstore/unset-removes-entry", test_winstore_unset_removes_entry);
  g_test_add_func ("/winstore/unset-missing-entry-returns-false", test_winstore_unset_missing_entry_returns_false);
  g_test_add_func ("/winstore/get-missing-entry-returns-false", test_winstore_get_missing_entry_returns_false_and_leaves_out_untouched);
  g_test_add_func ("/winstore/set-ignores-invalid-xid", test_winstore_set_ignores_invalid_xid);

  g_test_add_func ("/winstore/load-missing-file", test_winstore_load_missing_file_degrades_to_empty_silently);
  g_test_add_func ("/winstore/load-unreadable-file", test_winstore_load_unreadable_file_degrades_to_empty_with_one_warning);
  g_test_add_func ("/winstore/load-malformed-content", test_winstore_load_malformed_content_degrades_to_empty_with_one_warning);
  g_test_add_func ("/winstore/load-null-path", test_winstore_load_null_path_degrades_to_empty_with_one_warning);

  g_test_add_func ("/winstore/save-then-load-round-trips", test_winstore_save_then_load_round_trips);
  g_test_add_func ("/winstore/save-creates-missing-parent-directory", test_winstore_save_creates_missing_parent_directory);
  g_test_add_func ("/winstore/entries-survive-a-missing-banner", test_winstore_entries_survive_a_missing_banner);
  g_test_add_func ("/winstore/record-with-mismatched-selector-is-skipped-whole", test_winstore_record_with_mismatched_selector_is_skipped_whole);
  g_test_add_func ("/winstore/state-rule-with-extra-declaration-is-skipped-whole", test_winstore_state_rule_with_extra_declaration_is_skipped_whole);

  g_test_add_func ("/winstore/render-reproduces-marker-class-and-declarations", test_winstore_render_reproduces_marker_class_and_declarations);
  g_test_add_func ("/winstore/render-of-empty-store-has-no-entries", test_winstore_render_of_empty_store_has_no_entries);
  g_test_add_func ("/winstore/render-alpha-is-locale-independent", test_winstore_render_alpha_is_locale_independent);

  g_test_add_func ("/winstore/xids-of-empty-store-returns-empty-array", test_winstore_xids_of_empty_store_returns_empty_array);
  g_test_add_func ("/winstore/xids-returns-stable-insertion-order", test_winstore_xids_returns_stable_insertion_order);
  g_test_add_func ("/winstore/xids-feeds-reconcile-unchanged", test_winstore_xids_feeds_reconcile_unchanged);

  g_test_add_func ("/winstore/reconcile-keeps-everything-when-live-list-not-obtained", test_winstore_reconcile_keeps_everything_when_live_list_not_obtained);
  g_test_add_func ("/winstore/reconcile-drops-entries-confirmed-absent", test_winstore_reconcile_drops_entries_confirmed_absent);
  g_test_add_func ("/winstore/reconcile-keeps-entries-present-in-live-list", test_winstore_reconcile_keeps_entries_present_in_live_list);
  g_test_add_func ("/winstore/reconcile-with-successfully-obtained-empty-list-keeps-everything", test_winstore_reconcile_with_successfully_obtained_empty_list_keeps_everything);
  g_test_add_func ("/winstore/reconcile-with-notready-snapshot-does-not-authorize-a-write", test_winstore_reconcile_with_notready_snapshot_does_not_authorize_a_write);
  g_test_add_func ("/winstore/reconcile-returns-zero-on-empty-store", test_winstore_reconcile_returns_zero_on_empty_store);

  return g_test_run ();
}
