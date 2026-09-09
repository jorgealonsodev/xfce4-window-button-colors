/* lc-css.c — see lc-css.h */
#include "lc-css.h"

gchar *
lc_css_rule_new (const gchar *selector, const LcColor *color, const LcStyle *style)
{
  gchar *css_color;
  gchar *rule;

  if (selector == NULL || color == NULL || style == NULL)
    return NULL;

  css_color = lc_color_to_css (color);

  rule = g_strdup_printf (
    "%s {\n"
    "  background-color: %s;\n"
    "  border-radius: %dpx;\n"
    "  margin: %dpx;\n"
    "}\n",
    selector, css_color, style->corner_radius, style->margin);

  g_free (css_color);

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
