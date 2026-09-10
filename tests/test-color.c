/* tests/test-color.c — RED test for lc_color_parse / lc_color_to_css /
 * lc_color_to_hex round-trip on valid and malformed "#rrggbbaa" input, and
 * for lc_color_derive_state (design.md D19).
 */
#include <glib.h>
#include <locale.h>
#include <math.h>
#include <string.h>
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

/* --- lc_color_derive_state (design.md D19) ------------------------------ */

/* Composites color over an opaque background (the standard "over" operator)
 * and returns the summed per-channel delta between a and b. Used to assert
 * that two derived colors are visibly distinct even when their alpha
 * differs, not just when their raw RGB channels differ. */
static gdouble
composited_delta_over (const LcColor *a, const LcColor *b, guint8 bg)
{
  gdouble aa = a->a / 255.0;
  gdouble ba = b->a / 255.0;
  gdouble ar = a->r * aa + bg * (1.0 - aa);
  gdouble ag = a->g * aa + bg * (1.0 - aa);
  gdouble ab = a->b * aa + bg * (1.0 - aa);
  gdouble br = b->r * ba + bg * (1.0 - ba);
  gdouble bgc = b->g * ba + bg * (1.0 - ba);
  gdouble bb = b->b * ba + bg * (1.0 - ba);

  return fabs (ar - br) + fabs (ag - bgc) + fabs (ab - bb);
}

/* Composited delta is affine in the background value whenever the two
 * colors' alpha differ, so a single fixed background can coincide with the
 * point where the two composited results cross and cancel out — a false
 * "invisible" reading that says nothing about real panels. A real panel
 * background is overwhelmingly close to one of the two extremes (a light
 * theme or a dark theme), so require distinctness against at least one of
 * pure black and pure white rather than picking one arbitrary background. */
static gdouble
composited_delta (const LcColor *a, const LcColor *b)
{
  gdouble over_black = composited_delta_over (a, b, 0x00);
  gdouble over_white = composited_delta_over (a, b, 0xFF);

  return MAX (over_black, over_white);
}

/* Minimum acceptable composited delta between any two of {base, hover,
 * active}, against the more favorable of a black or white panel background.
 * Chosen below the smallest delta the D19 constants actually produce across
 * the sampled grid (10.0, at fully opaque colors near a pivot boundary), so
 * this is a regression guard, not a tight bound. */
#define MIN_COMPOSITED_DELTA 5.0

static void
test_derive_state_opaque_mid_tone_hover_lighter_active_darker (void)
{
  LcColor base = { 128, 96, 160, 255 };
  LcColor hover = lc_color_derive_state (&base, LC_STATE_HOVER);
  LcColor active = lc_color_derive_state (&base, LC_STATE_ACTIVE);

  /* Ordinary mid-tone: hover moves toward white, active toward black. */
  g_assert_cmpint (hover.r, >, base.r);
  g_assert_cmpint (hover.g, >, base.g);
  g_assert_cmpint (hover.b, >, base.b);
  g_assert_cmpint (active.r, <, base.r);
  g_assert_cmpint (active.g, <, base.g);
  g_assert_cmpint (active.b, <, base.b);

  /* Fully opaque base: alpha blend toward opaque is a no-op. */
  g_assert_cmpint (hover.a, ==, 255);
  g_assert_cmpint (active.a, ==, 255);
}

static void
test_derive_state_pure_white_both_darker_and_ordered (void)
{
  LcColor base = { 255, 255, 255, 255 };
  LcColor hover = lc_color_derive_state (&base, LC_STATE_HOVER);
  LcColor active = lc_color_derive_state (&base, LC_STATE_ACTIVE);

  /* A flat "+lightness" bump would be a no-op here; both states must move
   * toward black instead, with active moving further than hover. */
  g_assert_cmpint (hover.r, <, base.r);
  g_assert_cmpint (active.r, <, hover.r);
}

static void
test_derive_state_pure_black_both_lighter_and_ordered (void)
{
  LcColor base = { 0, 0, 0, 255 };
  LcColor hover = lc_color_derive_state (&base, LC_STATE_HOVER);
  LcColor active = lc_color_derive_state (&base, LC_STATE_ACTIVE);

  /* Cannot go darker than black: both states move toward white, with
   * active moving further than hover so they stay distinguishable. */
  g_assert_cmpint (hover.r, >, base.r);
  g_assert_cmpint (active.r, >, hover.r);
}

static void
test_derive_state_near_transparent_alpha_rises_toward_opaque (void)
{
  LcColor base = { 100, 150, 200, 13 }; /* alpha ~= 0.05 */
  LcColor hover = lc_color_derive_state (&base, LC_STATE_HOVER);
  LcColor active = lc_color_derive_state (&base, LC_STATE_ACTIVE);

  /* Without an alpha blend the composited difference would be ~1%, which
   * is the exact hover-invisible regression D17/D19 fix. */
  g_assert_cmpint (hover.a, >, base.a);
  g_assert_cmpint (active.a, >, hover.a);
}

static void
test_derive_state_fully_opaque_base_keeps_alpha_at_max (void)
{
  LcColor base = { 10, 200, 30, 255 };
  LcColor hover = lc_color_derive_state (&base, LC_STATE_HOVER);
  LcColor active = lc_color_derive_state (&base, LC_STATE_ACTIVE);

  g_assert_cmpint (hover.a, ==, 255);
  g_assert_cmpint (active.a, ==, 255);
}

static void
test_derive_state_alpha_never_exceeds_max_or_decreases (void)
{
  const guint8 alphas[] = { 0, 1, 13, 64, 128, 200, 254, 255 };

  for (gsize i = 0; i < G_N_ELEMENTS (alphas); i++)
    {
      LcColor base = { 40, 90, 210, alphas[i] };
      LcColor hover = lc_color_derive_state (&base, LC_STATE_HOVER);
      LcColor active = lc_color_derive_state (&base, LC_STATE_ACTIVE);

      g_assert_cmpint (hover.a, >=, base.a);
      g_assert_cmpint (active.a, >=, base.a);
      g_assert_cmpint (hover.a, <=, 255);
      g_assert_cmpint (active.a, <=, 255);
    }
}

static void
test_derive_state_light_pivot_flips_hover_target (void)
{
  /* Gray 200 -> L = 200/255 = 0.784, below LC_STATE_LIGHT_PIVOT (0.85):
   * hover still targets white and stays lighter than base. */
  LcColor below_pivot = { 200, 200, 200, 255 };
  LcColor hover_below = lc_color_derive_state (&below_pivot, LC_STATE_HOVER);
  g_assert_cmpint (hover_below.r, >, below_pivot.r);

  /* Gray 220 -> L = 220/255 = 0.8627, above LC_STATE_LIGHT_PIVOT (0.85):
   * hover flips to targeting black and goes darker than base. */
  LcColor above_pivot = { 220, 220, 220, 255 };
  LcColor hover_above = lc_color_derive_state (&above_pivot, LC_STATE_HOVER);
  g_assert_cmpint (hover_above.r, <, above_pivot.r);
}

static void
test_derive_state_dark_pivot_flips_active_target (void)
{
  /* Gray 50 -> L = 50/255 = 0.196, above LC_STATE_DARK_PIVOT (0.15):
   * active still targets black and stays darker than base. */
  LcColor above_pivot = { 50, 50, 50, 255 };
  LcColor active_above = lc_color_derive_state (&above_pivot, LC_STATE_ACTIVE);
  g_assert_cmpint (active_above.r, <, above_pivot.r);

  /* Gray 30 -> L = 30/255 = 0.1176, below LC_STATE_DARK_PIVOT (0.15):
   * active flips to targeting white and goes lighter than base. */
  LcColor below_pivot = { 30, 30, 30, 255 };
  LcColor active_below = lc_color_derive_state (&below_pivot, LC_STATE_ACTIVE);
  g_assert_cmpint (active_below.r, >, below_pivot.r);
}

/* The invariant the design pins: over a sampled colour grid, base, hover
 * and active are pairwise distinct by at least a minimum composited delta.
 * The grid spans the full channel range plus alpha extremes, and explicitly
 * includes pure white, pure black, a nearly transparent colour and a fully
 * opaque mid-tone. */
static void
test_derive_state_pairwise_distinct_over_grid (void)
{
  const guint8 channel_samples[] = { 0, 1, 40, 63, 90, 128, 160, 200, 210, 220, 254, 255 };
  const guint8 alpha_samples[] = { 13, 64, 128, 255 }; /* includes ~0.05 and fully opaque */

  for (gsize ri = 0; ri < G_N_ELEMENTS (channel_samples); ri++)
    for (gsize gi = 0; gi < G_N_ELEMENTS (channel_samples); gi++)
      for (gsize bi = 0; bi < G_N_ELEMENTS (channel_samples); bi++)
        for (gsize ai = 0; ai < G_N_ELEMENTS (alpha_samples); ai++)
          {
            LcColor base = { channel_samples[ri], channel_samples[gi],
                              channel_samples[bi], alpha_samples[ai] };
            LcColor hover = lc_color_derive_state (&base, LC_STATE_HOVER);
            LcColor active = lc_color_derive_state (&base, LC_STATE_ACTIVE);

            g_assert_cmpfloat (composited_delta (&base, &hover), >=, MIN_COMPOSITED_DELTA);
            g_assert_cmpfloat (composited_delta (&base, &active), >=, MIN_COMPOSITED_DELTA);
            g_assert_cmpfloat (composited_delta (&hover, &active), >=, MIN_COMPOSITED_DELTA);
          }
}

static void
test_derive_state_explicit_extremes_pairwise_distinct (void)
{
  const LcColor grid[] = {
    { 255, 255, 255, 255 }, /* pure white */
    { 0, 0, 0, 255 },       /* pure black */
    { 100, 150, 200, 13 },  /* nearly transparent */
    { 128, 96, 160, 255 },  /* fully opaque mid-tone */
  };

  for (gsize i = 0; i < G_N_ELEMENTS (grid); i++)
    {
      LcColor base = grid[i];
      LcColor hover = lc_color_derive_state (&base, LC_STATE_HOVER);
      LcColor active = lc_color_derive_state (&base, LC_STATE_ACTIVE);

      g_assert_cmpfloat (composited_delta (&base, &hover), >=, MIN_COMPOSITED_DELTA);
      g_assert_cmpfloat (composited_delta (&base, &active), >=, MIN_COMPOSITED_DELTA);
      g_assert_cmpfloat (composited_delta (&hover, &active), >=, MIN_COMPOSITED_DELTA);
    }
}

/* Regression guard: lc_color_to_css() must format alpha locale-independently.
 *
 * printf's "%f" family honours LC_NUMERIC, and gtk_init() calls
 * setlocale(LC_ALL, ""), so inside the real xfce4-panel process under any
 * comma-decimal locale this emitted "rgba(233,30,140,0,85)". GTK then
 * rejects the entire rule ("Expected ')' in color definition") and the
 * lifecycle's step-7 GError path discards it with only a log warning, so
 * every colour silently fails to paint. Verified against a real
 * GtkCssProvider before the fix. */
static void
test_color_to_css_alpha_is_locale_independent (void)
{
  static const gchar *comma_locales[] = {
    "es_ES.UTF-8", "es_ES.utf8", "de_DE.UTF-8", "fr_FR.UTF-8", NULL
  };
  gchar *saved = g_strdup (setlocale (LC_NUMERIC, NULL));
  const gchar *applied = NULL;
  LcColor color;
  gchar *css;

  for (gsize i = 0; comma_locales[i] != NULL && applied == NULL; i++)
    applied = setlocale (LC_NUMERIC, comma_locales[i]);

  g_assert_true (lc_color_parse ("#e91e8cd9", &color));
  css = lc_color_to_css (&color);

  /* Holds under every locale, including the C fallback when no
   * comma-decimal locale is installed on the build machine. */
  g_assert_nonnull (css);
  g_assert_cmpstr (css, ==, "rgba(233,30,140,0.85)");

  g_free (css);
  setlocale (LC_NUMERIC, saved);
  g_free (saved);

  if (applied == NULL)
    g_test_message ("no comma-decimal locale installed; assertion still held under C");
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
  g_test_add_func ("/color/to-css-alpha-is-locale-independent",
                   test_color_to_css_alpha_is_locale_independent);

  g_test_add_func ("/color/derive-state/opaque-mid-tone-hover-lighter-active-darker",
                    test_derive_state_opaque_mid_tone_hover_lighter_active_darker);
  g_test_add_func ("/color/derive-state/pure-white-both-darker-and-ordered",
                    test_derive_state_pure_white_both_darker_and_ordered);
  g_test_add_func ("/color/derive-state/pure-black-both-lighter-and-ordered",
                    test_derive_state_pure_black_both_lighter_and_ordered);
  g_test_add_func ("/color/derive-state/near-transparent-alpha-rises-toward-opaque",
                    test_derive_state_near_transparent_alpha_rises_toward_opaque);
  g_test_add_func ("/color/derive-state/fully-opaque-base-keeps-alpha-at-max",
                    test_derive_state_fully_opaque_base_keeps_alpha_at_max);
  g_test_add_func ("/color/derive-state/alpha-never-exceeds-max-or-decreases",
                    test_derive_state_alpha_never_exceeds_max_or_decreases);
  g_test_add_func ("/color/derive-state/light-pivot-flips-hover-target",
                    test_derive_state_light_pivot_flips_hover_target);
  g_test_add_func ("/color/derive-state/dark-pivot-flips-active-target",
                    test_derive_state_dark_pivot_flips_active_target);
  g_test_add_func ("/color/derive-state/pairwise-distinct-over-grid",
                    test_derive_state_pairwise_distinct_over_grid);
  g_test_add_func ("/color/derive-state/explicit-extremes-pairwise-distinct",
                    test_derive_state_explicit_extremes_pairwise_distinct);

  return g_test_run ();
}
