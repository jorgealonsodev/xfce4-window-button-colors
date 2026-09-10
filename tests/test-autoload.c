/* tests/test-autoload.c — lc-autoload's pure list transforms
 * (normalize/join/is_enabled/add/remove), design.md D1. Every transform
 * here operates on an in-memory `gchar **` list only — no xfconf I/O and
 * no knowledge of the `/Gtk/Modules` on-disk shape at all. The shape
 * orchestration itself (LcAutoloadBackend, lc_autoload_set_enabled(),
 * D1's full table) is Phase 3 and is NOT tested here.
 *
 * House rule this file exists to enforce: every test that stands in for
 * "another application's autoload module" uses a THIRD-PARTY name, not
 * this project's own — a test built only around our own module name
 * cannot distinguish "removed the right entry" from "removed everything"
 * when there is nothing else in the list to disturb.
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

  return g_test_run ();
}
