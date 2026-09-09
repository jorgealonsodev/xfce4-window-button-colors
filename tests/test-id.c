/* tests/test-id.c — RED test for lc_id_is_valid / lc_id_parse / lc_id_selector_new
 *
 * Covers launcher-color-persistence spec "colors.css Selector Schema" (RF-4):
 * the generated selector is always the two-level
 * "#launcher-<id> #launcher-arrow" shape, never the single-level
 * "#launcher-<id>" alone and never "#launcher-button".
 */
#include <glib.h>
#include <string.h>
#include "lc-id.h"

static void
test_id_is_valid_nonnegative (void)
{
  g_assert_true (lc_id_is_valid (0));
  g_assert_true (lc_id_is_valid (13));
}

static void
test_id_is_valid_rejects_negative (void)
{
  g_assert_false (lc_id_is_valid (-1));
}

static void
test_id_parse_valid (void)
{
  gint out = -1;
  g_assert_true (lc_id_parse ("12", &out));
  g_assert_cmpint (out, ==, 12);
}

static void
test_id_parse_zero (void)
{
  gint out = -1;
  g_assert_true (lc_id_parse ("0", &out));
  g_assert_cmpint (out, ==, 0);
}

static void
test_id_parse_rejects_null (void)
{
  gint out = -1;
  g_assert_false (lc_id_parse (NULL, &out));
}

static void
test_id_parse_rejects_empty (void)
{
  gint out = -1;
  g_assert_false (lc_id_parse ("", &out));
}

static void
test_id_parse_rejects_negative_sign (void)
{
  gint out = -1;
  g_assert_false (lc_id_parse ("-1", &out));
}

static void
test_id_parse_rejects_non_digit (void)
{
  gint out = -1;
  g_assert_false (lc_id_parse ("12a", &out));
  g_assert_false (lc_id_parse ("a12", &out));
}

static void
test_id_parse_rejects_whitespace (void)
{
  gint out = -1;
  g_assert_false (lc_id_parse (" 12", &out));
  g_assert_false (lc_id_parse ("12 ", &out));
}

static void
test_id_selector_new_two_level_shape (void)
{
  gchar *selector = lc_id_selector_new (13);
  g_assert_cmpstr (selector, ==, "#launcher-13 #launcher-arrow");
  g_free (selector);
}

static void
test_id_selector_never_single_level_or_button (void)
{
  gchar *selector = lc_id_selector_new (7);

  g_assert_nonnull (strstr (selector, "#launcher-arrow"));
  g_assert_null (strstr (selector, "#launcher-button"));
  /* The ancestor half must be exactly "#launcher-7", not "#launcher-7"
   * alone as the whole selector. */
  g_assert_cmpstr (selector, !=, "#launcher-7");

  g_free (selector);
}

static void
test_id_selector_new_rejects_invalid_id (void)
{
  g_assert_null (lc_id_selector_new (-1));
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/id/is-valid-nonnegative", test_id_is_valid_nonnegative);
  g_test_add_func ("/id/is-valid-rejects-negative", test_id_is_valid_rejects_negative);
  g_test_add_func ("/id/parse-valid", test_id_parse_valid);
  g_test_add_func ("/id/parse-zero", test_id_parse_zero);
  g_test_add_func ("/id/parse-rejects-null", test_id_parse_rejects_null);
  g_test_add_func ("/id/parse-rejects-empty", test_id_parse_rejects_empty);
  g_test_add_func ("/id/parse-rejects-negative-sign", test_id_parse_rejects_negative_sign);
  g_test_add_func ("/id/parse-rejects-non-digit", test_id_parse_rejects_non_digit);
  g_test_add_func ("/id/parse-rejects-whitespace", test_id_parse_rejects_whitespace);
  g_test_add_func ("/id/selector-new-two-level-shape", test_id_selector_new_two_level_shape);
  g_test_add_func ("/id/selector-never-single-level-or-button", test_id_selector_never_single_level_or_button);
  g_test_add_func ("/id/selector-new-rejects-invalid-id", test_id_selector_new_rejects_invalid_id);

  return g_test_run ();
}
