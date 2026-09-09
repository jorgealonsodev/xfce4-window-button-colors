/* lc-css.c — see lc-css.h */
#include "lc-css.h"

gchar *
lc_css_rule_new (const gchar *selector, const LcColor *color, const LcStyle *style)
{
  LcColor hover_color;
  LcColor active_color;
  gchar *css_base;
  gchar *css_hover;
  gchar *css_active;
  gchar *rule;

  if (selector == NULL || color == NULL || style == NULL)
    return NULL;

  hover_color = lc_color_derive_state (color, LC_STATE_HOVER);
  active_color = lc_color_derive_state (color, LC_STATE_ACTIVE);

  css_base = lc_color_to_css (color);
  css_hover = lc_color_to_css (&hover_color);
  css_active = lc_color_to_css (&active_color);

  /* Geometry (border-radius, margin) and the theme-image suppression
   * (background-image: none) live on the base rule only, exactly once.
   * The state rules carry background-color and nothing else, and the
   * pseudo-class is appended directly to `selector`, so it always attaches
   * to the trailing element — never to the "#launcher-<id>" ancestor. */
  rule = g_strdup_printf (
    "%s {\n"
    "  background-image: none;\n"
    "  background-color: %s;\n"
    "  border-radius: %dpx;\n"
    "  margin: %dpx;\n"
    "}\n"
    "%s:hover {\n"
    "  background-color: %s;\n"
    "}\n"
    "%s:active {\n"
    "  background-color: %s;\n"
    "}\n",
    selector, css_base, style->corner_radius, style->margin,
    selector, css_hover,
    selector, css_active);

  g_free (css_base);
  g_free (css_hover);
  g_free (css_active);

  return rule;
}

gchar *
lc_css_render_rules (const gchar *const *rules, gsize n)
{
  GString *out;

  out = g_string_new (NULL);

  for (gsize i = 0; i < n; i++)
    {
      if (rules[i] != NULL)
        g_string_append (out, rules[i]);
    }

  return g_string_free (out, FALSE);
}
