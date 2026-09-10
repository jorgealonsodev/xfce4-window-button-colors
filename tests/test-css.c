/* tests/test-css.c — tests for lc_css_rule_new / lc_css_render_rules / LcStyle
 *
 * Covers launcher-color-persistence spec requirements "Generated Rule Shape
 * and Theme Override" (RF-3, RF-4), "Distinguishable Hover and Pressed
 * Feedback" (RF-3), and "Pseudo-Class Placement on the Selector Chain"
 * (RF-3, RF-4): every colored launcher renders exactly three rules — a base
 * rule carrying `background-image: none`, geometry and the base color, and
 * `:hover`/`:active` rules carrying only their derived `background-color`,
 * with the pseudo-class attached to the trailing selector element
 * (design.md D17/D19).
 */
#include <glib.h>
#include <string.h>
#include "lc-css.h"
#include "lc-color.h"

/* Returns a newly allocated copy of the rule block starting at the first
 * occurrence of "<anchor> {" in text, up to and including its closing brace.
 * Callers pass an anchor with no trailing whitespace so a base selector
 * anchor (ending directly in " {") cannot accidentally match inside a
 * longer state selector (which has ":hover"/":active" before " {"). */
static gchar *
extract_block (const gchar *text, const gchar *anchor)
{
  gchar *needle = g_strdup_printf ("%s {", anchor);
  const gchar *start = strstr (text, needle);
  const gchar *close;
  gchar *block;

  g_free (needle);

  if (start == NULL)
    return NULL;

  close = strchr (start, '}');
  if (close == NULL)
    return NULL;

  block = g_strndup (start, (gsize) (close - start + 1));
  return block;
}

static void
test_css_rule_new_renders_three_rules (void)
{
  LcColor color;
  LcStyle style = { .corner_radius = 6, .margin = 2 };
  gchar *selector;
  gchar *rendered;

  g_assert_true (lc_color_parse ("#e91e8cd9", &color));
  selector = g_strdup_printf ("#launcher-13 #launcher-arrow");
  rendered = lc_css_rule_new (selector, &color, &style);

  g_assert_nonnull (rendered);
  g_assert_nonnull (strstr (rendered, "#launcher-13 #launcher-arrow {"));
  g_assert_nonnull (strstr (rendered, "#launcher-13 #launcher-arrow:hover {"));
  g_assert_nonnull (strstr (rendered, "#launcher-13 #launcher-arrow:active {"));

  g_free (selector);
  g_free (rendered);
}

static void
test_css_rule_base_contains_background_image_none (void)
{
  LcColor color;
  LcStyle style = { .corner_radius = 6, .margin = 2 };
  gchar *selector;
  gchar *rendered;
  gchar *base_block;

  g_assert_true (lc_color_parse ("#e91e8cd9", &color));
  selector = g_strdup_printf ("#launcher-13 #launcher-arrow");
  rendered = lc_css_rule_new (selector, &color, &style);

  /* A generator that omits this on the base rule is exactly the failure
   * mode that made Adwaita render nothing while everything else looked
   * right (spike finding cited by design.md D17). */
  base_block = extract_block (rendered, selector);
  g_assert_nonnull (base_block);
  g_assert_nonnull (strstr (base_block, "background-image: none;"));
  g_assert_nonnull (strstr (base_block, "background-color: rgba(233,30,140,0.85);"));

  g_free (base_block);
  g_free (selector);
  g_free (rendered);
}

static void
test_css_rule_geometry_appears_exactly_once_on_base (void)
{
  LcColor color;
  LcStyle style = { .corner_radius = 6, .margin = 2 };
  gchar *selector;
  gchar *rendered;
  gchar *base_block;
  gchar *hover_selector;
  gchar *active_selector;
  gchar *hover_block;
  gchar *active_block;

  g_assert_true (lc_color_parse ("#e91e8cd9", &color));
  selector = g_strdup_printf ("#launcher-13 #launcher-arrow");
  rendered = lc_css_rule_new (selector, &color, &style);

  hover_selector = g_strdup_printf ("%s:hover", selector);
  active_selector = g_strdup_printf ("%s:active", selector);

  base_block = extract_block (rendered, selector);
  hover_block = extract_block (rendered, hover_selector);
  active_block = extract_block (rendered, active_selector);

  g_assert_nonnull (base_block);
  g_assert_nonnull (hover_block);
  g_assert_nonnull (active_block);

  /* border-radius and margin are geometry, not state: base only, and
   * absent from the hover/active blocks that carry colour only. */
  g_assert_nonnull (strstr (base_block, "border-radius: 6px;"));
  g_assert_nonnull (strstr (base_block, "margin: 2px;"));
  g_assert_null (strstr (hover_block, "border-radius"));
  g_assert_null (strstr (hover_block, "margin"));
  g_assert_null (strstr (active_block, "border-radius"));
  g_assert_null (strstr (active_block, "margin"));

  /* Exactly once across the whole rendered fragment, i.e. never repeated
   * on the state rules either. */
  g_assert_null (strstr (strstr (rendered, "border-radius") + strlen ("border-radius"), "border-radius"));
  g_assert_null (strstr (strstr (rendered, "margin:") + strlen ("margin:"), "margin:"));

  g_free (base_block);
  g_free (hover_block);
  g_free (active_block);
  g_free (hover_selector);
  g_free (active_selector);
  g_free (selector);
  g_free (rendered);
}

static void
test_css_rule_state_rules_carry_only_background_color (void)
{
  LcColor color;
  LcStyle style = { .corner_radius = 6, .margin = 2 };
  gchar *selector;
  gchar *rendered;
  gchar *hover_selector;
  gchar *active_selector;
  gchar *hover_block;
  gchar *active_block;

  g_assert_true (lc_color_parse ("#e91e8cd9", &color));
  selector = g_strdup_printf ("#launcher-13 #launcher-arrow");
  rendered = lc_css_rule_new (selector, &color, &style);

  hover_selector = g_strdup_printf ("%s:hover", selector);
  active_selector = g_strdup_printf ("%s:active", selector);

  hover_block = extract_block (rendered, hover_selector);
  active_block = extract_block (rendered, active_selector);

  g_assert_nonnull (hover_block);
  g_assert_nonnull (active_block);

  g_assert_nonnull (strstr (hover_block, "background-color:"));
  g_assert_null (strstr (hover_block, "background-image"));
  g_assert_null (strstr (hover_block, "border-radius"));
  g_assert_null (strstr (hover_block, "margin"));

  g_assert_nonnull (strstr (active_block, "background-color:"));
  g_assert_null (strstr (active_block, "background-image"));
  g_assert_null (strstr (active_block, "border-radius"));
  g_assert_null (strstr (active_block, "margin"));

  g_free (hover_block);
  g_free (active_block);
  g_free (hover_selector);
  g_free (active_selector);
  g_free (selector);
  g_free (rendered);
}

static void
test_css_rule_hover_and_active_colors_are_derived_and_distinct (void)
{
  LcColor color;
  LcColor expected_hover;
  LcColor expected_active;
  LcStyle style = { .corner_radius = 6, .margin = 2 };
  gchar *selector;
  gchar *rendered;
  gchar *expected_hover_css;
  gchar *expected_active_css;
  gchar *expected_base_css;

  g_assert_true (lc_color_parse ("#e91e8cd9", &color));
  selector = g_strdup_printf ("#launcher-13 #launcher-arrow");
  rendered = lc_css_rule_new (selector, &color, &style);

  expected_hover = lc_color_derive_state (&color, LC_STATE_HOVER);
  expected_active = lc_color_derive_state (&color, LC_STATE_ACTIVE);

  expected_base_css = lc_color_to_css (&color);
  expected_hover_css = lc_color_to_css (&expected_hover);
  expected_active_css = lc_color_to_css (&expected_active);

  /* Never equal to the base or to each other, and each equals exactly what
   * lc_color_derive_state() computes — the hover/active colors are derived
   * at render time, never a stored or hand-picked value. */
  g_assert_cmpstr (expected_hover_css, !=, expected_base_css);
  g_assert_cmpstr (expected_active_css, !=, expected_base_css);
  g_assert_cmpstr (expected_active_css, !=, expected_hover_css);

  g_assert_nonnull (strstr (rendered, expected_hover_css));
  g_assert_nonnull (strstr (rendered, expected_active_css));

  g_free (expected_base_css);
  g_free (expected_hover_css);
  g_free (expected_active_css);
  g_free (selector);
  g_free (rendered);
}

static void
test_css_rule_never_single_level_or_button_selector (void)
{
  LcColor color;
  LcStyle style = { .corner_radius = 0, .margin = 0 };
  gchar *selector;
  gchar *rendered;

  g_assert_true (lc_color_parse ("#000000ff", &color));
  selector = g_strdup_printf ("#launcher-7 #launcher-arrow");
  rendered = lc_css_rule_new (selector, &color, &style);

  g_assert_null (strstr (rendered, "#launcher-button"));
  g_assert_nonnull (strstr (rendered, "#launcher-7 #launcher-arrow"));

  g_free (selector);
  g_free (rendered);
}

static void
test_css_rule_pseudo_class_attached_to_descendant (void)
{
  LcColor color;
  LcStyle style = { .corner_radius = 6, .margin = 2 };
  gchar *selector;
  gchar *rendered;

  g_assert_true (lc_color_parse ("#e91e8cd9", &color));
  selector = g_strdup_printf ("#launcher-13 #launcher-arrow");
  rendered = lc_css_rule_new (selector, &color, &style);

  g_assert_nonnull (strstr (rendered, "#launcher-13 #launcher-arrow:hover"));
  g_assert_nonnull (strstr (rendered, "#launcher-13 #launcher-arrow:active"));

  g_free (selector);
  g_free (rendered);
}

static void
test_css_rule_pseudo_class_never_attached_to_ancestor (void)
{
  LcColor color;
  LcStyle style = { .corner_radius = 6, .margin = 2 };
  gchar *selector;
  gchar *rendered;

  g_assert_true (lc_color_parse ("#e91e8cd9", &color));
  selector = g_strdup_printf ("#launcher-13 #launcher-arrow");
  rendered = lc_css_rule_new (selector, &color, &style);

  /* GTK3 resolves a pseudo-class against the element it is attached to;
   * attaching it to the ancestor produces a selector that silently matches
   * nothing, the same failure shape as the "#launcher-button" prohibition. */
  g_assert_null (strstr (rendered, "#launcher-13:hover"));
  g_assert_null (strstr (rendered, "#launcher-13:active"));

  g_free (selector);
  g_free (rendered);
}

static void
test_css_render_rules_concatenates_in_order (void)
{
  const gchar *rules[] = {
    "#launcher-1 #launcher-arrow {\n  background-color: rgba(0,0,0,1.00);\n}\n",
    "#launcher-2 #launcher-arrow {\n  background-color: rgba(1,1,1,1.00);\n}\n",
  };
  gchar *rendered = lc_css_render_rules (rules, 2);
  const gchar *first_pos;
  const gchar *second_pos;

  g_assert_nonnull (rendered);
  first_pos = strstr (rendered, "#launcher-1");
  second_pos = strstr (rendered, "#launcher-2");
  g_assert_nonnull (first_pos);
  g_assert_nonnull (second_pos);
  g_assert_true (first_pos < second_pos);

  g_free (rendered);
}

static void
test_css_render_rules_empty (void)
{
  gchar *rendered = lc_css_render_rules (NULL, 0);
  g_assert_nonnull (rendered);
  g_assert_cmpstr (rendered, ==, "");
  g_free (rendered);
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/css/rule-new-renders-three-rules", test_css_rule_new_renders_three_rules);
  g_test_add_func ("/css/rule-base-contains-background-image-none",
                    test_css_rule_base_contains_background_image_none);
  g_test_add_func ("/css/rule-geometry-appears-exactly-once-on-base",
                    test_css_rule_geometry_appears_exactly_once_on_base);
  g_test_add_func ("/css/rule-state-rules-carry-only-background-color",
                    test_css_rule_state_rules_carry_only_background_color);
  g_test_add_func ("/css/rule-hover-and-active-colors-are-derived-and-distinct",
                    test_css_rule_hover_and_active_colors_are_derived_and_distinct);
  g_test_add_func ("/css/rule-never-single-level-or-button-selector",
                    test_css_rule_never_single_level_or_button_selector);
  g_test_add_func ("/css/rule-pseudo-class-attached-to-descendant",
                    test_css_rule_pseudo_class_attached_to_descendant);
  g_test_add_func ("/css/rule-pseudo-class-never-attached-to-ancestor",
                    test_css_rule_pseudo_class_never_attached_to_ancestor);
  g_test_add_func ("/css/render-rules-concatenates-in-order", test_css_render_rules_concatenates_in_order);
  g_test_add_func ("/css/render-rules-empty", test_css_render_rules_empty);

  return g_test_run ();
}
