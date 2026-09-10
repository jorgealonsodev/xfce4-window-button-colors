/* tests/test-autoload.c — lc-autoload's pure list transforms
 * (normalize/join/is_enabled/add/remove) AND the D1 shape-table
 * orchestration (LcAutoloadBackend, lc_autoload_set_enabled()),
 * design.md D1.
 *
 * The pure transforms operate on an in-memory `gchar **` list only — no
 * xfconf I/O and no knowledge of the `/Gtk/Modules` on-disk shape at
 * all (Phase 2). lc_autoload_set_enabled() is the orchestration layer
 * that reads a shape via a two-function LcAutoloadBackend vtable,
 * applies add/remove, and decides the write-back shape per D1's table
 * (Phase 3). It never touches xfconf itself — the fake backend below
 * stands in for it, so the whole policy is testable as pure logic. The
 * real xfconf-backed implementation is Phase 4's
 * src/settings/lc-autoload-xfconf.c, entirely out of scope here.
 *
 * House rule this file exists to enforce: every test that stands in for
 * "another application's autoload module" uses a THIRD-PARTY name, not
 * this project's own — a test built only around our own module name
 * cannot distinguish "removed the right entry" from "removed everything"
 * when there is nothing else in the list to disturb. For the
 * lc_autoload_set_enabled() tests below this rule is load-bearing, not
 * stylistic: this is the function that writes to a session-wide key
 * OTHER applications share, so every test that has a third-party entry
 * to check asserts its byte-for-byte survival explicitly (task 3.13).
 * The handful of rows where no third party is present by construction
 * (the key is ABSENT, or holds ONLY our module) are marked N/A in their
 * own comment, since there is nothing there to disturb.
 */
#include <glib.h>
#include <stdarg.h>
#include <string.h>
#include "lc-autoload.h"

#define OUR_MODULE "xfce4-window-button-colors"

/* Builds a newly allocated, NULL-terminated gchar** from a NULL-terminated
 * varargs list of string literals, e.g. modules_new ("a", "b", NULL).
 * Caller owns the returned array (g_strfreev()). */
static gchar **
modules_new (const gchar *first, ...)
{
  GPtrArray *out = g_ptr_array_new ();

  if (first != NULL)
    {
      va_list args;
      const gchar *arg;

      g_ptr_array_add (out, g_strdup (first));

      va_start (args, first);
      while ((arg = va_arg (args, const gchar *)) != NULL)
        g_ptr_array_add (out, g_strdup (arg));
      va_end (args);
    }

  g_ptr_array_add (out, NULL);

  return (gchar **) g_ptr_array_free (out, FALSE);
}

/* Asserts `actual` holds exactly the strings named by the NULL-terminated
 * varargs list that follows it, in that exact order — same length, same
 * content, same position. Fails loudly (not silently) on any mismatch,
 * including a length mismatch either direction. */
static void
assert_modules_equal (gchar **actual, const gchar *first, ...)
{
  GPtrArray *expected = g_ptr_array_new ();
  guint i;

  if (first != NULL)
    {
      va_list args;
      const gchar *arg;

      g_ptr_array_add (expected, (gpointer) first);

      va_start (args, first);
      while ((arg = va_arg (args, const gchar *)) != NULL)
        g_ptr_array_add (expected, (gpointer) arg);
      va_end (args);
    }

  g_assert_nonnull (actual);

  for (i = 0; i < expected->len; i++)
    {
      g_assert_nonnull (actual[i]);
      g_assert_cmpstr (actual[i], ==, (const gchar *) g_ptr_array_index (expected, i));
    }
  g_assert_null (actual[expected->len]);

  g_ptr_array_free (expected, TRUE);
}

/* ==== lc_autoload_add — spec: "Add to an empty list" (task 2.1) ======= */

static void
test_autoload_add_to_empty_list_contains_only_added_module (void)
{
  /* GIVEN an empty module list, WHEN lc_autoload_add is called with our
   * module name, THEN the resulting list contains exactly our module. */
  gchar **empty = modules_new (NULL);
  gchar **result = lc_autoload_add ((const gchar *const *) empty, OUR_MODULE);

  assert_modules_equal (result, OUR_MODULE, NULL);

  g_strfreev (empty);
  g_strfreev (result);
}

static void
test_autoload_add_treats_null_list_the_same_as_empty (void)
{
  /* `modules` may legitimately be NULL (e.g. the very first read before
   * any list has ever been built) — it must be treated identically to
   * an empty array, not crash or be mistaken for "already enabled". */
  gchar **result = lc_autoload_add (NULL, OUR_MODULE);

  assert_modules_equal (result, OUR_MODULE, NULL);

  g_strfreev (result);
}

static void
test_autoload_add_appends_to_existing_list_preserving_order (void)
{
  /* Triangulation: a non-empty starting list, with THIRD-PARTY entries
   * (not our own module name), must survive unchanged and in order, and
   * our module is appended after them. */
  gchar **modules = modules_new ("canberra-gtk-module", "some-other-vendor-module", NULL);
  gchar **result = lc_autoload_add ((const gchar *const *) modules, OUR_MODULE);

  assert_modules_equal (result, "canberra-gtk-module", "some-other-vendor-module", OUR_MODULE, NULL);

  g_strfreev (modules);
  g_strfreev (result);
}

static void
test_autoload_add_is_idempotent_no_duplicate_entry (void)
{
  /* design.md D1 / prompt requirement: "add is idempotent — adding
   * twice must not produce a duplicate." Our module already sits
   * between two third-party entries; adding it again must leave the
   * list byte-for-byte the same, not append a second copy. */
  gchar **modules = modules_new ("canberra-gtk-module", OUR_MODULE, "some-other-vendor-module", NULL);
  gchar **result = lc_autoload_add ((const gchar *const *) modules, OUR_MODULE);

  assert_modules_equal (result, "canberra-gtk-module", OUR_MODULE, "some-other-vendor-module", NULL);

  g_strfreev (modules);
  g_strfreev (result);
}

/* ==== lc_autoload_remove — spec: "Remove preserves other entries"
 * (task 2.2) =========================================================== */

static void
test_autoload_remove_preserves_third_party_entries_in_order (void)
{
  /* GIVEN a list containing our module and at least one other module,
   * WHEN lc_autoload_remove is called, THEN our module is removed and
   * every other entry survives, unchanged, in its original order.
   *
   * Deliberately uses THIRD-PARTY names, not this project's own, on
   * both sides of our entry: a test built only around our own module
   * name cannot tell "removed the right entry" apart from "removed
   * everything", because there is nothing else left to check. */
  gchar **modules = modules_new ("canberra-gtk-module", OUR_MODULE, "some-other-vendor-module", NULL);
  gchar **result = lc_autoload_remove ((const gchar *const *) modules, OUR_MODULE);

  assert_modules_equal (result, "canberra-gtk-module", "some-other-vendor-module", NULL);

  g_strfreev (modules);
  g_strfreev (result);
}

static void
test_autoload_remove_preserves_a_single_third_party_entry (void)
{
  /* Triangulation with a different shape: our module first, one
   * third-party entry after it. */
  gchar **modules = modules_new (OUR_MODULE, "some-other-vendor-module", NULL);
  gchar **result = lc_autoload_remove ((const gchar *const *) modules, OUR_MODULE);

  assert_modules_equal (result, "some-other-vendor-module", NULL);

  g_strfreev (modules);
  g_strfreev (result);
}

static void
test_autoload_remove_of_only_entry_leaves_empty_list (void)
{
  /* GIVEN a list holding only our module, WHEN removed, THEN the
   * result is empty — no leftover, no NULL-pointer result. */
  gchar **modules = modules_new (OUR_MODULE, NULL);
  gchar **result = lc_autoload_remove ((const gchar *const *) modules, OUR_MODULE);

  assert_modules_equal (result, NULL);

  g_strfreev (modules);
  g_strfreev (result);
}

/* ==== lc_autoload_remove — spec: "Remove when absent is a no-op"
 * (task 2.3) =========================================================== */

static void
test_autoload_remove_when_absent_is_a_noop_preserving_third_parties (void)
{
  /* GIVEN a list that does not contain our module, WHEN
   * lc_autoload_remove is called, THEN the list is unchanged — no
   * removal is reported, and this must not be a `remove` that silently
   * discards someone else's entry. */
  gchar **modules = modules_new ("canberra-gtk-module", "some-other-vendor-module", NULL);
  gchar **result = lc_autoload_remove ((const gchar *const *) modules, OUR_MODULE);

  assert_modules_equal (result, "canberra-gtk-module", "some-other-vendor-module", NULL);

  g_strfreev (modules);
  g_strfreev (result);
}

static void
test_autoload_remove_from_empty_list_is_a_noop (void)
{
  /* Triangulation: the degenerate empty-list case must also be a
   * no-op, not a crash and not an error result. */
  gchar **empty = modules_new (NULL);
  gchar **result = lc_autoload_remove ((const gchar *const *) empty, OUR_MODULE);

  assert_modules_equal (result, NULL);

  g_strfreev (empty);
  g_strfreev (result);
}

static void
test_autoload_remove_from_null_list_is_a_noop (void)
{
  /* `modules` may be NULL (nothing has ever been read yet); removing
   * from it must degrade to an empty result, not crash. */
  gchar **result = lc_autoload_remove (NULL, OUR_MODULE);

  assert_modules_equal (result, NULL);

  g_strfreev (result);
}

/* ==== lc_autoload_normalize — colon-splitting, trim, drop-empties
 * (task 2.4) =========================================================== */

static void
test_autoload_normalize_splits_colon_joined_scalar_into_two_entries (void)
{
  /* THE bug this function exists to prevent: GTK's own `gtk-modules`
   * value is colon-separated, so "canberra-gtk-module:xfce4-window-
   * button-colors" is TWO entries, not one opaque token. Without this
   * split, lc_autoload_remove() would silently no-op (the compound
   * string never exact-matches our bare module name) and
   * lc_autoload_add() would append a duplicate compound entry — both
   * are third-party-entry corruption in disguise. */
  gchar **result = lc_autoload_normalize ("canberra-gtk-module:" OUR_MODULE);

  assert_modules_equal (result, "canberra-gtk-module", OUR_MODULE, NULL);

  g_strfreev (result);
}

static void
test_autoload_normalize_trims_whitespace_around_each_element (void)
{
  gchar **result = lc_autoload_normalize ("  canberra-gtk-module :  " OUR_MODULE "  ");

  assert_modules_equal (result, "canberra-gtk-module", OUR_MODULE, NULL);

  g_strfreev (result);
}

static void
test_autoload_normalize_drops_empty_elements_from_double_colons (void)
{
  /* Threat-matrix case: "::" and a trailing ':' both produce empty
   * pieces after splitting — they must be dropped, never kept as
   * empty-string "modules". */
  gchar **result = lc_autoload_normalize ("canberra-gtk-module::" OUR_MODULE ":");

  assert_modules_equal (result, "canberra-gtk-module", OUR_MODULE, NULL);

  g_strfreev (result);
}

static void
test_autoload_normalize_drops_whitespace_only_element (void)
{
  /* Threat-matrix case: a whitespace-only element between two
   * delimiters must trim down to empty and be dropped, same as "::". */
  gchar **result = lc_autoload_normalize (OUR_MODULE ":   :canberra-gtk-module");

  assert_modules_equal (result, OUR_MODULE, "canberra-gtk-module", NULL);

  g_strfreev (result);
}

static void
test_autoload_normalize_null_scalar_returns_non_null_empty_array (void)
{
  gchar **result = lc_autoload_normalize (NULL);

  g_assert_nonnull (result);
  assert_modules_equal (result, NULL);

  g_strfreev (result);
}

static void
test_autoload_normalize_empty_string_returns_non_null_empty_array (void)
{
  gchar **result = lc_autoload_normalize ("");

  g_assert_nonnull (result);
  assert_modules_equal (result, NULL);

  g_strfreev (result);
}

/* ==== lc_autoload_join — the SCALAR write-back inverse of normalize
 * (task 2.4) =========================================================== */

static void
test_autoload_join_produces_colon_joined_scalar (void)
{
  gchar **modules = modules_new ("canberra-gtk-module", OUR_MODULE, NULL);
  gchar *joined = lc_autoload_join ((const gchar *const *) modules);

  g_assert_cmpstr (joined, ==, "canberra-gtk-module:" OUR_MODULE);

  g_strfreev (modules);
  g_free (joined);
}

static void
test_autoload_join_round_trips_through_normalize (void)
{
  /* join() then normalize() must reconstruct the exact same list —
   * this is what makes join() the genuine inverse of normalize() for
   * the SCALAR write-back shape (D1), not merely "some string". */
  gchar **modules = modules_new ("canberra-gtk-module", OUR_MODULE, "some-other-vendor-module", NULL);
  gchar *joined = lc_autoload_join ((const gchar *const *) modules);
  gchar **round_tripped = lc_autoload_normalize (joined);

  assert_modules_equal (round_tripped, "canberra-gtk-module", OUR_MODULE, "some-other-vendor-module", NULL);

  g_strfreev (modules);
  g_free (joined);
  g_strfreev (round_tripped);
}

static void
test_autoload_join_of_empty_or_null_list_returns_empty_string (void)
{
  gchar **empty = modules_new (NULL);
  gchar *joined_empty = lc_autoload_join ((const gchar *const *) empty);
  gchar *joined_null = lc_autoload_join (NULL);

  g_assert_nonnull (joined_empty);
  g_assert_cmpstr (joined_empty, ==, "");
  g_assert_nonnull (joined_null);
  g_assert_cmpstr (joined_null, ==, "");

  g_strfreev (empty);
  g_free (joined_empty);
  g_free (joined_null);
}

/* ==== lc_autoload_is_enabled — exact-match only (task 2.4) ============ */

static void
test_autoload_is_enabled_true_on_exact_match (void)
{
  gchar **modules = modules_new ("canberra-gtk-module", OUR_MODULE, NULL);

  g_assert_true (lc_autoload_is_enabled ((const gchar *const *) modules, OUR_MODULE));

  g_strfreev (modules);
}

static void
test_autoload_is_enabled_false_on_suffix_near_miss (void)
{
  /* design requirement: "ours-extra" must not count as "ours" — a
   * substring/prefix match here would target the wrong module for a
   * later disable. */
  gchar **modules = modules_new (OUR_MODULE "-extra", NULL);

  g_assert_false (lc_autoload_is_enabled ((const gchar *const *) modules, OUR_MODULE));

  g_strfreev (modules);
}

static void
test_autoload_is_enabled_false_on_prefix_near_miss (void)
{
  /* Mirror case: a name that merely ENDS with ours must not match
   * either — exact-match cuts both directions. */
  gchar **modules = modules_new ("extra-" OUR_MODULE, NULL);

  g_assert_false (lc_autoload_is_enabled ((const gchar *const *) modules, OUR_MODULE));

  g_strfreev (modules);
}

static void
test_autoload_is_enabled_false_when_absent_among_third_parties (void)
{
  gchar **modules = modules_new ("canberra-gtk-module", "some-other-vendor-module", NULL);

  g_assert_false (lc_autoload_is_enabled ((const gchar *const *) modules, OUR_MODULE));

  g_strfreev (modules);
}

static void
test_autoload_is_enabled_false_for_null_list (void)
{
  g_assert_false (lc_autoload_is_enabled (NULL, OUR_MODULE));
}

/* ==== Fake LcAutoloadBackend — lc_autoload_set_enabled() orchestration
 * tests (task 3.1) ======================================================
 *
 * A test double for the two-function LcAutoloadBackend vtable (D1). It
 * never touches xfconf — the real implementation lives in Phase 4's
 * src/settings/lc-autoload-xfconf.c, out of scope here. This fake exists
 * so the whole D1 shape table, its idempotency rules, and its refusal
 * behaviour can be exercised as pure logic.
 *
 * Per the backend contract documented in lc-autoload.h, `read()` hands
 * back the ALREADY NORMALIZED module list for SCALAR/ARRAY shapes (a
 * real backend would call lc_autoload_normalize() itself on a raw
 * scalar before returning it) — so this fake's canned `read_modules`
 * fixture data is set up the same way.
 *
 * `write()` records every call — the load-bearing evidence for D1 —
 * and additionally simulates persistence: on a SUCCESSFUL write, the
 * fake's "current" read state is updated to exactly what was written,
 * so a SECOND lc_autoload_set_enabled() call against the same fake
 * genuinely exercises "twice in a row" against real prior output,
 * rather than replaying stale fixture data.
 */
typedef struct {
  /* --- read() canned response --- */
  gboolean         read_should_succeed;
  LcAutoloadShape  read_shape;
  gchar          **read_modules;   /* already-normalized fixture data, or NULL for ABSENT */

  /* --- write() canned response --- */
  gboolean         write_should_succeed;

  /* --- write() call recording --- */
  guint            write_attempt_count;  /* incremented on EVERY call, success or failure */
  gboolean         write_called;         /* TRUE only once a call has SUCCEEDED */
  guint            write_call_count;     /* count of SUCCESSFUL calls only */
  LcAutoloadShape  write_shape;          /* shape of the most recent successful call */
  gchar          **write_modules;        /* deep copy of the most recent successful call's list */
} FakeBackend;

static gboolean
fake_backend_read (gpointer user_data, LcAutoloadShape *out_shape, gchar ***out_modules)
{
  FakeBackend *fake = user_data;

  if (!fake->read_should_succeed)
    return FALSE;

  *out_shape = fake->read_shape;
  /* Hand back a FRESH copy: lc_autoload_set_enabled() is expected to
   * own and free whatever it receives, exactly like a real backend. */
  *out_modules = g_strdupv (fake->read_modules);
  return TRUE;
}

static gboolean
fake_backend_write (gpointer user_data, LcAutoloadShape shape, const gchar *const *modules)
{
  FakeBackend *fake = user_data;

  fake->write_attempt_count++;

  if (!fake->write_should_succeed)
    return FALSE;

  fake->write_called = TRUE;
  fake->write_call_count++;
  fake->write_shape = shape;
  g_strfreev (fake->write_modules);
  fake->write_modules = g_strdupv ((gchar **) modules);

  /* Simulate persistence for the "enabling/disabling twice" scenarios. */
  g_strfreev (fake->read_modules);
  fake->read_shape = shape;
  fake->read_modules = g_strdupv (fake->write_modules);

  return TRUE;
}

static const LcAutoloadBackend fake_backend_vtable = {
  fake_backend_read,
  fake_backend_write,
};

static void
fake_backend_init (FakeBackend *fake)
{
  memset (fake, 0, sizeof (*fake));
  fake->read_should_succeed = TRUE;
  fake->write_should_succeed = TRUE;
}

static void
fake_backend_clear (FakeBackend *fake)
{
  g_strfreev (fake->read_modules);
  g_strfreev (fake->write_modules);
}

/* ==== lc_autoload_set_enabled — D1 table, "Key absent, enable"
 * (task 3.2) =========================================================== */

static void
test_autoload_set_enabled_key_absent_enable_creates_key_with_our_module (void)
{
  /* GIVEN /Gtk/Modules is absent, WHEN the module is enabled, THEN the
   * key is created holding exactly our module. N/A for task 3.13: the
   * key is genuinely absent, so there is no third-party entry that
   * could exist here to disturb. */
  FakeBackend fake;
  LcAutoloadResult result;

  fake_backend_init (&fake);
  fake.read_shape = LC_AUTOLOAD_SHAPE_ABSENT;
  fake.read_modules = NULL;

  result = lc_autoload_set_enabled (&fake_backend_vtable, &fake, OUR_MODULE, TRUE);

  g_assert_cmpint (result, ==, LC_AUTOLOAD_OK);
  g_assert_true (fake.write_called);
  g_assert_cmpint (fake.write_shape, ==, LC_AUTOLOAD_SHAPE_SCALAR);
  assert_modules_equal (fake.write_modules, OUR_MODULE, NULL);

  fake_backend_clear (&fake);
}

/* ==== lc_autoload_set_enabled — D1 table, "scalar holding only our
 * module, enable is idempotent" (task 3.3) ============================= */

static void
test_autoload_set_enabled_scalar_only_ours_enable_is_idempotent (void)
{
  /* GIVEN /Gtk/Modules is a scalar equal to our module name, WHEN
   * enabled again, THEN the result still holds exactly our module, with
   * no duplication — proven here by asserting NO WRITE happens at all.
   * N/A for task 3.13: the scalar holds ONLY our module, so there is no
   * third-party entry present to disturb. */
  FakeBackend fake;
  LcAutoloadResult result;

  fake_backend_init (&fake);
  fake.read_shape = LC_AUTOLOAD_SHAPE_SCALAR;
  fake.read_modules = modules_new (OUR_MODULE, NULL);

  result = lc_autoload_set_enabled (&fake_backend_vtable, &fake, OUR_MODULE, TRUE);

  g_assert_cmpint (result, ==, LC_AUTOLOAD_NO_CHANGE);
  g_assert_false (fake.write_called);
  g_assert_cmpuint (fake.write_attempt_count, ==, 0);

  fake_backend_clear (&fake);
}

/* ==== lc_autoload_set_enabled — D1 table, "scalar holding a different
 * module, enable preserves it" (task 3.4) =============================== */

static void
test_autoload_set_enabled_scalar_with_other_module_enable_preserves_it (void)
{
  /* GIVEN /Gtk/Modules is a scalar holding a THIRD-PARTY module's name,
   * WHEN our module is enabled, THEN the result holds both that other
   * module's entry, byte-for-byte unchanged, and our module, and no
   * entry is duplicated. */
  FakeBackend fake;
  LcAutoloadResult result;

  fake_backend_init (&fake);
  fake.read_shape = LC_AUTOLOAD_SHAPE_SCALAR;
  fake.read_modules = modules_new ("canberra-gtk-module", NULL);

  result = lc_autoload_set_enabled (&fake_backend_vtable, &fake, OUR_MODULE, TRUE);

  g_assert_cmpint (result, ==, LC_AUTOLOAD_OK);
  g_assert_true (fake.write_called);
  g_assert_cmpint (fake.write_shape, ==, LC_AUTOLOAD_SHAPE_SCALAR);
  /* Task 3.13's load-bearing assertion: the third-party entry survives
   * byte-for-byte, not merely "a write happened." */
  assert_modules_equal (fake.write_modules, "canberra-gtk-module", OUR_MODULE, NULL);

  fake_backend_clear (&fake);
}

/* ==== lc_autoload_set_enabled — D1 table, "array with third-party
 * modules, enable preserves them" (task 3.5) ============================ */

static void
test_autoload_set_enabled_array_with_third_parties_enable_preserves_them (void)
{
  /* GIVEN /Gtk/Modules is an array holding SEVERAL third-party modules,
   * WHEN our module is enabled, THEN our module is added, every
   * third-party entry survives unchanged, and our module appears
   * exactly once. */
  FakeBackend fake;
  LcAutoloadResult result;

  fake_backend_init (&fake);
  fake.read_shape = LC_AUTOLOAD_SHAPE_ARRAY;
  fake.read_modules = modules_new ("canberra-gtk-module", "some-other-vendor-module", NULL);

  result = lc_autoload_set_enabled (&fake_backend_vtable, &fake, OUR_MODULE, TRUE);

  g_assert_cmpint (result, ==, LC_AUTOLOAD_OK);
  g_assert_true (fake.write_called);
  g_assert_cmpint (fake.write_shape, ==, LC_AUTOLOAD_SHAPE_ARRAY);
  /* Task 3.13: both third-party entries survive byte-for-byte. */
  assert_modules_equal (fake.write_modules, "canberra-gtk-module", "some-other-vendor-module", OUR_MODULE, NULL);

  fake_backend_clear (&fake);
}

/* ==== lc_autoload_set_enabled — D1 table, "Enabling twice is
 * idempotent" (task 3.6) ================================================= */

static void
test_autoload_set_enabled_enabling_twice_is_idempotent (void)
{
  /* GIVEN our module is already present, in any shape (here: an ARRAY
   * alongside third-party entries), WHEN enable is invoked a SECOND
   * time, THEN the stored value is unchanged from after the first
   * enable — no duplicate entry, and the second write() call NEVER
   * happens. This is a genuine two-call sequence, not a single-call
   * proxy: the fake's write() persists its own state (see the struct
   * comment above), so the second call's read() sees the real result
   * of the first write. */
  FakeBackend fake;
  LcAutoloadResult first, second;

  fake_backend_init (&fake);
  fake.read_shape = LC_AUTOLOAD_SHAPE_ARRAY;
  fake.read_modules = modules_new ("canberra-gtk-module", "some-other-vendor-module", NULL);

  first = lc_autoload_set_enabled (&fake_backend_vtable, &fake, OUR_MODULE, TRUE);
  g_assert_cmpint (first, ==, LC_AUTOLOAD_OK);
  g_assert_cmpuint (fake.write_call_count, ==, 1);
  assert_modules_equal (fake.write_modules, "canberra-gtk-module", "some-other-vendor-module", OUR_MODULE, NULL);

  second = lc_autoload_set_enabled (&fake_backend_vtable, &fake, OUR_MODULE, TRUE);
  g_assert_cmpint (second, ==, LC_AUTOLOAD_NO_CHANGE);
  /* The load-bearing assertion: still 1, not 2 — write() was NOT
   * called again. Task 3.13: both third-party entries are still there,
   * unchanged, in the last successfully-written value. */
  g_assert_cmpuint (fake.write_call_count, ==, 1);
  assert_modules_equal (fake.write_modules, "canberra-gtk-module", "some-other-vendor-module", OUR_MODULE, NULL);

  fake_backend_clear (&fake);
}

/* ==== lc_autoload_set_enabled — D1 table, "scalar holding only our
 * module, disable clears it" (task 3.7) ================================= */

static void
test_autoload_set_enabled_scalar_only_ours_disable_clears_key (void)
{
  /* GIVEN /Gtk/Modules is a scalar equal to our module name, WHEN
   * disabled, THEN the key ends absent (matching the README's
   * documented reset command). N/A for task 3.13: the scalar holds
   * ONLY our module — there is no third party present to disturb. */
  FakeBackend fake;
  LcAutoloadResult result;

  fake_backend_init (&fake);
  fake.read_shape = LC_AUTOLOAD_SHAPE_SCALAR;
  fake.read_modules = modules_new (OUR_MODULE, NULL);

  result = lc_autoload_set_enabled (&fake_backend_vtable, &fake, OUR_MODULE, FALSE);

  g_assert_cmpint (result, ==, LC_AUTOLOAD_OK);
  g_assert_true (fake.write_called);
  g_assert_cmpint (fake.write_shape, ==, LC_AUTOLOAD_SHAPE_ABSENT);
  g_assert_null (fake.write_modules);

  fake_backend_clear (&fake);
}

/* ==== lc_autoload_set_enabled — D1 table, "array with third-party
 * modules, disable removes only ours" (task 3.8) ========================= */

static void
test_autoload_set_enabled_array_with_third_parties_disable_removes_only_ours (void)
{
  /* GIVEN /Gtk/Modules is an array holding third-party modules AND
   * ours, WHEN our module is disabled, THEN only our module is removed
   * and every third-party entry survives, byte-for-byte unchanged. */
  FakeBackend fake;
  LcAutoloadResult result;

  fake_backend_init (&fake);
  fake.read_shape = LC_AUTOLOAD_SHAPE_ARRAY;
  fake.read_modules = modules_new ("canberra-gtk-module", OUR_MODULE, "some-other-vendor-module", NULL);

  result = lc_autoload_set_enabled (&fake_backend_vtable, &fake, OUR_MODULE, FALSE);

  g_assert_cmpint (result, ==, LC_AUTOLOAD_OK);
  g_assert_true (fake.write_called);
  g_assert_cmpint (fake.write_shape, ==, LC_AUTOLOAD_SHAPE_ARRAY);
  /* Task 3.13: both third-party entries survive byte-for-byte and in
   * their original relative order; only OUR_MODULE is gone. */
  assert_modules_equal (fake.write_modules, "canberra-gtk-module", "some-other-vendor-module", NULL);

  fake_backend_clear (&fake);
}

/* ==== lc_autoload_set_enabled — D1 table, "Disabling when we are not
 * present is a no-op" (task 3.9) ========================================= */

static void
test_autoload_set_enabled_disabling_when_not_present_is_a_noop (void)
{
  /* GIVEN /Gtk/Modules does not contain our module (an array of
   * THIRD-PARTY entries only), WHEN disable is invoked, THEN the stored
   * value is unchanged and no write occurs. */
  FakeBackend fake;
  LcAutoloadResult result;

  fake_backend_init (&fake);
  fake.read_shape = LC_AUTOLOAD_SHAPE_ARRAY;
  fake.read_modules = modules_new ("canberra-gtk-module", "some-other-vendor-module", NULL);

  result = lc_autoload_set_enabled (&fake_backend_vtable, &fake, OUR_MODULE, FALSE);

  g_assert_cmpint (result, ==, LC_AUTOLOAD_NO_CHANGE);
  g_assert_false (fake.write_called);
  g_assert_cmpuint (fake.write_attempt_count, ==, 0);
  /* Task 3.13: proven two ways — write() was never even attempted, AND
   * the fixture's own "current" state is still exactly what it was. */
  assert_modules_equal (fake.read_modules, "canberra-gtk-module", "some-other-vendor-module", NULL);

  fake_backend_clear (&fake);
}

static void
test_autoload_set_enabled_disabling_when_key_absent_is_a_noop (void)
{
  /* Same scenario, degenerate shape: the key is absent entirely, not
   * merely missing our entry among others. Explicitly required by the
   * apply prompt alongside the no-op-when-absent RED case above. N/A
   * for task 3.13: nothing at all is present to disturb. */
  FakeBackend fake;
  LcAutoloadResult result;

  fake_backend_init (&fake);
  fake.read_shape = LC_AUTOLOAD_SHAPE_ABSENT;
  fake.read_modules = NULL;

  result = lc_autoload_set_enabled (&fake_backend_vtable, &fake, OUR_MODULE, FALSE);

  g_assert_cmpint (result, ==, LC_AUTOLOAD_NO_CHANGE);
  g_assert_false (fake.write_called);
  g_assert_cmpuint (fake.write_attempt_count, ==, 0);

  fake_backend_clear (&fake);
}

/* ==== lc_autoload_set_enabled — unrecognised GType and hostile
 * /Gtk/Modules content (task 3.10, threat matrix) ======================= */

static void
test_autoload_set_enabled_unrecognised_gtype_refuses_write (void)
{
  /* Threat matrix: /Gtk/Modules is user- and third-party-writable, so
   * it may legitimately hold a value that is neither a scalar string
   * nor a string array. D1: refuse the write outright rather than
   * guess a conversion. */
  FakeBackend fake;
  LcAutoloadResult result;

  fake_backend_init (&fake);
  fake.read_shape = LC_AUTOLOAD_SHAPE_UNKNOWN;
  fake.read_modules = NULL;

  result = lc_autoload_set_enabled (&fake_backend_vtable, &fake, OUR_MODULE, TRUE);

  g_assert_cmpint (result, ==, LC_AUTOLOAD_UNSUPPORTED_SHAPE);
  g_assert_false (fake.write_called);
  g_assert_cmpuint (fake.write_attempt_count, ==, 0);

  fake_backend_clear (&fake);
}

static void
test_autoload_set_enabled_empty_array_enable_proceeds_normally (void)
{
  /* Threat matrix: an empty array is a legitimate, if unusual, on-disk
   * state (e.g. every module was removed by hand) — it must not be
   * mistaken for ABSENT or refused as UNSUPPORTED_SHAPE. N/A for task
   * 3.13: the array is empty, so there is no third party present. */
  FakeBackend fake;
  LcAutoloadResult result;

  fake_backend_init (&fake);
  fake.read_shape = LC_AUTOLOAD_SHAPE_ARRAY;
  fake.read_modules = modules_new (NULL);

  result = lc_autoload_set_enabled (&fake_backend_vtable, &fake, OUR_MODULE, TRUE);

  g_assert_cmpint (result, ==, LC_AUTOLOAD_OK);
  g_assert_true (fake.write_called);
  g_assert_cmpint (fake.write_shape, ==, LC_AUTOLOAD_SHAPE_ARRAY);
  assert_modules_equal (fake.write_modules, OUR_MODULE, NULL);

  fake_backend_clear (&fake);
}

static void
test_autoload_set_enabled_array_with_blank_element_survives_byte_for_byte (void)
{
  /* Threat matrix: "array containing NULL elements". A NULL-terminated
   * gchar** cannot literally hold a mid-array NULL pointer without that
   * NULL being read back as the terminator, so the hostile shape this
   * rules out is an element that reads back as an EMPTY STRING — what a
   * malformed or partially-typed xfconf array entry degrades to. D1
   * does not editorialize about what a third-party entry looks like:
   * it survives whatever shape it is found in, including a blank one —
   * dropping it would be exactly the kind of "helpful" reinterpretation
   * D1 exists to rule out. */
  FakeBackend fake;
  LcAutoloadResult result;

  fake_backend_init (&fake);
  fake.read_shape = LC_AUTOLOAD_SHAPE_ARRAY;
  fake.read_modules = modules_new ("canberra-gtk-module", "", "some-other-vendor-module", NULL);

  result = lc_autoload_set_enabled (&fake_backend_vtable, &fake, OUR_MODULE, TRUE);

  g_assert_cmpint (result, ==, LC_AUTOLOAD_OK);
  g_assert_true (fake.write_called);
  g_assert_cmpint (fake.write_shape, ==, LC_AUTOLOAD_SHAPE_ARRAY);
  /* Task 3.13: both real third-party entries AND the blank element
   * survive byte-for-byte, in order; only our module is newly added. */
  assert_modules_equal (fake.write_modules, "canberra-gtk-module", "", "some-other-vendor-module", OUR_MODULE, NULL);

  fake_backend_clear (&fake);
}

static void
test_autoload_set_enabled_hostile_scalar_survives_after_read_normalizes_it (void)
{
  /* Threat matrix: "::" and a whitespace-only element are two of the
   * hostile shapes lc_autoload_normalize() rules out (task 2.4). A
   * correct backend's read() calls lc_autoload_normalize() itself on
   * the raw scalar before handing the list to lc_autoload_set_enabled()
   * — see lc-autoload.h: read() reports "for SCALAR/ARRAY, the
   * NORMALIZED module list". This test proves the COMPOSITION of that
   * contract: fixture data built the same way a real backend would
   * build it from "canberra-gtk-module::  :some-other-vendor-module:"
   * survives our module's enable untouched, with no phantom empty
   * entries introduced by the orchestration layer itself. */
  FakeBackend fake;
  LcAutoloadResult result;
  gchar **hostile_scalar_normalized =
      lc_autoload_normalize ("canberra-gtk-module::  :some-other-vendor-module:");

  fake_backend_init (&fake);
  fake.read_shape = LC_AUTOLOAD_SHAPE_SCALAR;
  fake.read_modules = hostile_scalar_normalized;  /* ownership passes to fake_backend_clear() */

  result = lc_autoload_set_enabled (&fake_backend_vtable, &fake, OUR_MODULE, TRUE);

  g_assert_cmpint (result, ==, LC_AUTOLOAD_OK);
  g_assert_true (fake.write_called);
  g_assert_cmpint (fake.write_shape, ==, LC_AUTOLOAD_SHAPE_SCALAR);
  /* Task 3.13: both third-party entries survive, with the "::" and
   * whitespace-only element correctly gone (dropped by normalize, not
   * by this orchestration layer) rather than surviving as garbage. */
  assert_modules_equal (fake.write_modules, "canberra-gtk-module", "some-other-vendor-module", OUR_MODULE, NULL);

  fake_backend_clear (&fake);
}

/* ==== lc_autoload_set_enabled — read()/write() I/O failure
 * (task 3.11) ============================================================= */

static void
test_autoload_set_enabled_read_failure_returns_read_failed (void)
{
  /* GIVEN backend->read() reports failure, THEN
   * LC_AUTOLOAD_READ_FAILED is returned and write() is never attempted.
   * N/A for task 3.13: a failed read means no data — including no
   * third party — was ever observed to begin with. */
  FakeBackend fake;
  LcAutoloadResult result;

  fake_backend_init (&fake);
  fake.read_should_succeed = FALSE;

  result = lc_autoload_set_enabled (&fake_backend_vtable, &fake, OUR_MODULE, TRUE);

  g_assert_cmpint (result, ==, LC_AUTOLOAD_READ_FAILED);
  g_assert_false (fake.write_called);
  g_assert_cmpuint (fake.write_attempt_count, ==, 0);

  fake_backend_clear (&fake);
}

static void
test_autoload_set_enabled_write_failure_returns_write_failed (void)
{
  /* GIVEN backend->write() reports failure, THEN
   * LC_AUTOLOAD_WRITE_FAILED is returned. Even though the intended
   * write would have preserved the third-party entry, WRITE_FAILED
   * means nothing was actually committed: the fake's own "current"
   * state (read_modules) is untouched, still holding the third-party
   * entry exactly as it was, because the fake only updates it inside a
   * SUCCESSFUL write() (task 3.13's survival evidence for this row). */
  FakeBackend fake;
  LcAutoloadResult result;

  fake_backend_init (&fake);
  fake.read_shape = LC_AUTOLOAD_SHAPE_SCALAR;
  fake.read_modules = modules_new ("canberra-gtk-module", NULL);
  fake.write_should_succeed = FALSE;

  result = lc_autoload_set_enabled (&fake_backend_vtable, &fake, OUR_MODULE, TRUE);

  g_assert_cmpint (result, ==, LC_AUTOLOAD_WRITE_FAILED);
  g_assert_cmpuint (fake.write_attempt_count, ==, 1);
  g_assert_false (fake.write_called);
  assert_modules_equal (fake.read_modules, "canberra-gtk-module", NULL);

  fake_backend_clear (&fake);
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/autoload/add/to-empty-list-contains-only-added-module",
                    test_autoload_add_to_empty_list_contains_only_added_module);
  g_test_add_func ("/autoload/add/treats-null-list-the-same-as-empty",
                    test_autoload_add_treats_null_list_the_same_as_empty);
  g_test_add_func ("/autoload/add/appends-to-existing-list-preserving-order",
                    test_autoload_add_appends_to_existing_list_preserving_order);
  g_test_add_func ("/autoload/add/is-idempotent-no-duplicate-entry",
                    test_autoload_add_is_idempotent_no_duplicate_entry);

  g_test_add_func ("/autoload/remove/preserves-third-party-entries-in-order",
                    test_autoload_remove_preserves_third_party_entries_in_order);
  g_test_add_func ("/autoload/remove/preserves-a-single-third-party-entry",
                    test_autoload_remove_preserves_a_single_third_party_entry);
  g_test_add_func ("/autoload/remove/of-only-entry-leaves-empty-list",
                    test_autoload_remove_of_only_entry_leaves_empty_list);
  g_test_add_func ("/autoload/remove/when-absent-is-a-noop-preserving-third-parties",
                    test_autoload_remove_when_absent_is_a_noop_preserving_third_parties);
  g_test_add_func ("/autoload/remove/from-empty-list-is-a-noop",
                    test_autoload_remove_from_empty_list_is_a_noop);
  g_test_add_func ("/autoload/remove/from-null-list-is-a-noop",
                    test_autoload_remove_from_null_list_is_a_noop);

  g_test_add_func ("/autoload/normalize/splits-colon-joined-scalar-into-two-entries",
                    test_autoload_normalize_splits_colon_joined_scalar_into_two_entries);
  g_test_add_func ("/autoload/normalize/trims-whitespace-around-each-element",
                    test_autoload_normalize_trims_whitespace_around_each_element);
  g_test_add_func ("/autoload/normalize/drops-empty-elements-from-double-colons",
                    test_autoload_normalize_drops_empty_elements_from_double_colons);
  g_test_add_func ("/autoload/normalize/drops-whitespace-only-element",
                    test_autoload_normalize_drops_whitespace_only_element);
  g_test_add_func ("/autoload/normalize/null-scalar-returns-non-null-empty-array",
                    test_autoload_normalize_null_scalar_returns_non_null_empty_array);
  g_test_add_func ("/autoload/normalize/empty-string-returns-non-null-empty-array",
                    test_autoload_normalize_empty_string_returns_non_null_empty_array);

  g_test_add_func ("/autoload/join/produces-colon-joined-scalar",
                    test_autoload_join_produces_colon_joined_scalar);
  g_test_add_func ("/autoload/join/round-trips-through-normalize",
                    test_autoload_join_round_trips_through_normalize);
  g_test_add_func ("/autoload/join/of-empty-or-null-list-returns-empty-string",
                    test_autoload_join_of_empty_or_null_list_returns_empty_string);

  g_test_add_func ("/autoload/is-enabled/true-on-exact-match",
                    test_autoload_is_enabled_true_on_exact_match);
  g_test_add_func ("/autoload/is-enabled/false-on-suffix-near-miss",
                    test_autoload_is_enabled_false_on_suffix_near_miss);
  g_test_add_func ("/autoload/is-enabled/false-on-prefix-near-miss",
                    test_autoload_is_enabled_false_on_prefix_near_miss);
  g_test_add_func ("/autoload/is-enabled/false-when-absent-among-third-parties",
                    test_autoload_is_enabled_false_when_absent_among_third_parties);
  g_test_add_func ("/autoload/is-enabled/false-for-null-list",
                    test_autoload_is_enabled_false_for_null_list);

  g_test_add_func ("/autoload/set-enabled/key-absent-enable-creates-key-with-our-module",
                    test_autoload_set_enabled_key_absent_enable_creates_key_with_our_module);
  g_test_add_func ("/autoload/set-enabled/scalar-only-ours-enable-is-idempotent",
                    test_autoload_set_enabled_scalar_only_ours_enable_is_idempotent);
  g_test_add_func ("/autoload/set-enabled/scalar-with-other-module-enable-preserves-it",
                    test_autoload_set_enabled_scalar_with_other_module_enable_preserves_it);
  g_test_add_func ("/autoload/set-enabled/array-with-third-parties-enable-preserves-them",
                    test_autoload_set_enabled_array_with_third_parties_enable_preserves_them);
  g_test_add_func ("/autoload/set-enabled/enabling-twice-is-idempotent",
                    test_autoload_set_enabled_enabling_twice_is_idempotent);
  g_test_add_func ("/autoload/set-enabled/scalar-only-ours-disable-clears-key",
                    test_autoload_set_enabled_scalar_only_ours_disable_clears_key);
  g_test_add_func ("/autoload/set-enabled/array-with-third-parties-disable-removes-only-ours",
                    test_autoload_set_enabled_array_with_third_parties_disable_removes_only_ours);
  g_test_add_func ("/autoload/set-enabled/disabling-when-not-present-is-a-noop",
                    test_autoload_set_enabled_disabling_when_not_present_is_a_noop);
  g_test_add_func ("/autoload/set-enabled/disabling-when-key-absent-is-a-noop",
                    test_autoload_set_enabled_disabling_when_key_absent_is_a_noop);
  g_test_add_func ("/autoload/set-enabled/unrecognised-gtype-refuses-write",
                    test_autoload_set_enabled_unrecognised_gtype_refuses_write);
  g_test_add_func ("/autoload/set-enabled/empty-array-enable-proceeds-normally",
                    test_autoload_set_enabled_empty_array_enable_proceeds_normally);
  g_test_add_func ("/autoload/set-enabled/array-with-blank-element-survives-byte-for-byte",
                    test_autoload_set_enabled_array_with_blank_element_survives_byte_for_byte);
  g_test_add_func ("/autoload/set-enabled/hostile-scalar-survives-after-read-normalizes-it",
                    test_autoload_set_enabled_hostile_scalar_survives_after_read_normalizes_it);
  g_test_add_func ("/autoload/set-enabled/read-failure-returns-read-failed",
                    test_autoload_set_enabled_read_failure_returns_read_failed);
  g_test_add_func ("/autoload/set-enabled/write-failure-returns-write-failed",
                    test_autoload_set_enabled_write_failure_returns_write_failed);

  return g_test_run ();
}
