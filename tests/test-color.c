/* tests/test-color.c — RED test for lc_color_parse / lc_color_to_css /
 * lc_color_to_hex round-trip on valid and malformed "#rrggbbaa" input.
 */
#include <glib.h>
#include "lc-color.h"

static void
test_color_parse_valid (void)
{
  LcColor c;

  g_assert_true (lc_color_parse ("#e91e8cd9", &c));
  g_assert_cmpint (c.r, ==, 0xe9);
  g_assert_cmpint (c.g, ==, 0x1e);
  g_assert_cmpint (c.b, ==, 0x8c);
  g_assert_cmpint (c.a, ==, 0xd9);
}

static void
test_color_parse_case_insensitive (void)
{
  LcColor lower;
  LcColor upper;

  g_assert_true (lc_color_parse ("#e91e8cd9", &lower));
  g_assert_true (lc_color_parse ("#E91E8CD9", &upper));
  g_assert_cmpint (lower.r, ==, upper.r);
  g_assert_cmpint (lower.g, ==, upper.g);
  g_assert_cmpint (lower.b, ==, upper.b);
  g_assert_cmpint (lower.a, ==, upper.a);
}

static void
test_color_parse_rejects_null (void)
{
  LcColor c;
  g_assert_false (lc_color_parse (NULL, &c));
}

static void
test_color_parse_rejects_missing_hash (void)
{
  LcColor c;
  g_assert_false (lc_color_parse ("e91e8cd9", &c));
}

static void
test_color_parse_rejects_wrong_length (void)
{
  LcColor c;
  /* 6 hex digits (no alpha) is not the "#rrggbbaa" contract. */
  g_assert_false (lc_color_parse ("#e91e8c", &c));
  /* Too long. */
  g_assert_false (lc_color_parse ("#e91e8cd9ff", &c));
}

static void
test_color_parse_rejects_non_hex (void)
{
  LcColor c;
  g_assert_false (lc_color_parse ("#zz1e8cd9", &c));
}

static void
test_color_parse_rejects_empty (void)
{
  LcColor c;
  g_assert_false (lc_color_parse ("", &c));
}

static void
test_color_to_hex_round_trip (void)
{
  LcColor c;
  gchar *hex;

  g_assert_true (lc_color_parse ("#e91e8cd9", &c));
  hex = lc_color_to_hex (&c);
  g_assert_cmpstr (hex, ==, "#e91e8cd9");
  g_free (hex);
}

static void
test_color_to_css_matches_design_example (void)
{
  LcColor c;
  gchar *css;

  g_assert_true (lc_color_parse ("#e91e8cd9", &c));
  css = lc_color_to_css (&c);
  /* design.md's colors.css example: rgba(233,30,140,0.85) */
  g_assert_cmpstr (css, ==, "rgba(233,30,140,0.85)");
  g_free (css);
}

static void
test_color_to_hex_null_returns_null (void)
{
  g_assert_null (lc_color_to_hex (NULL));
}

static void
test_color_to_css_null_returns_null (void)
{
  g_assert_null (lc_color_to_css (NULL));
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/color/parse-valid", test_color_parse_valid);
  g_test_add_func ("/color/parse-case-insensitive", test_color_parse_case_insensitive);
  g_test_add_func ("/color/parse-rejects-null", test_color_parse_rejects_null);
  g_test_add_func ("/color/parse-rejects-missing-hash", test_color_parse_rejects_missing_hash);
  g_test_add_func ("/color/parse-rejects-wrong-length", test_color_parse_rejects_wrong_length);
  g_test_add_func ("/color/parse-rejects-non-hex", test_color_parse_rejects_non_hex);
  g_test_add_func ("/color/parse-rejects-empty", test_color_parse_rejects_empty);
  g_test_add_func ("/color/to-hex-round-trip", test_color_to_hex_round_trip);
  g_test_add_func ("/color/to-css-matches-design-example", test_color_to_css_matches_design_example);
  g_test_add_func ("/color/to-hex-null-returns-null", test_color_to_hex_null_returns_null);
  g_test_add_func ("/color/to-css-null-returns-null", test_color_to_css_null_returns_null);

  return g_test_run ();
}
