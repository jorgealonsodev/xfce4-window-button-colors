/* tests/test-css.c — RED test for lc_css_rule_new / lc_css_render_rules / LcStyle
 *
 * Covers launcher-color-persistence spec "colors.css Selector Schema" (RF-4)
 * scenario "Generated rule uses the two-level selector": a launcher with
 * unique id 13 and colour rgba(233,30,140,0.85) renders
 * "#launcher-13 #launcher-arrow { background-color: rgba(233,30,140,0.85); ... }".
 */
#include <glib.h>
#include <string.h>
#include "lc-css.h"
#include "lc-color.h"
#include "lc-id.h"

static void
test_css_rule_new_matches_design_example (void)
{
  LcColor color;
  LcStyle style = { .corner_radius = 6, .margin = 2 };
  gchar *selector;
  gchar *rule;

  g_assert_true (lc_color_parse ("#e91e8cd9", &color));
  selector = lc_id_selector_new (13);
  rule = lc_css_rule_new (selector, &color, &style);

  g_assert_nonnull (rule);
  g_assert_nonnull (strstr (rule, "#launcher-13 #launcher-arrow"));
  g_assert_nonnull (strstr (rule, "background-color: rgba(233,30,140,0.85);"));
  g_assert_nonnull (strstr (rule, "border-radius: 6px;"));
  g_assert_nonnull (strstr (rule, "margin: 2px;"));

  g_free (selector);
  g_free (rule);
}

static void
test_css_rule_never_single_level_or_button_selector (void)
{
  LcColor color;
  LcStyle style = { .corner_radius = 0, .margin = 0 };
  gchar *selector;
  gchar *rule;

  g_assert_true (lc_color_parse ("#000000ff", &color));
  selector = lc_id_selector_new (7);
  rule = lc_css_rule_new (selector, &color, &style);

  g_assert_null (strstr (rule, "#launcher-button"));
  g_assert_nonnull (strstr (rule, "#launcher-7 #launcher-arrow"));

  g_free (selector);
  g_free (rule);
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

  g_test_add_func ("/css/rule-new-matches-design-example", test_css_rule_new_matches_design_example);
  g_test_add_func ("/css/rule-never-single-level-or-button-selector", test_css_rule_never_single_level_or_button_selector);
  g_test_add_func ("/css/render-rules-concatenates-in-order", test_css_render_rules_concatenates_in_order);
  g_test_add_func ("/css/render-rules-empty", test_css_render_rules_empty);

  return g_test_run ();
}
