/* lc-css.h — CSS rule and rule-set fragment rendering. Links glib only.
 *
 * IMPORTANT — scope boundary (design.md D16):
 * This module renders plain CSS text fragments ONLY. It has nothing to do
 * with the opaque `LcDocument` type, which is a distinct type constructed
 * exclusively by `lc_store_render()` (src/core/lc-store.c, a later slice).
 * `LcDocument`'s entire reason to exist is that it has no
 * `_new_from_string()`/`_from_file()` constructor, so the CSS provider
 * (src/glue/lc-provider.c) cannot be handed anything but a rendered,
 * reconciled store. Nothing in this file may be used, wrapped, or renamed
 * to produce an `LcDocument` — doing so would quietly reopen the exact
 * bypass D16 exists to close. If a future change needs to feed the
 * provider, it goes through `lc_store_render()`, never through
 * `lc_css_render_rules()`.
 */
#ifndef LC_CSS_H
#define LC_CSS_H

#include <glib.h>
#include "lc-color.h"

G_BEGIN_DECLS

/* Global style values re-emitted into every rule (settings.ini, RF-7).
 * corner-radius and margin are expected to already be clamped (0-12) by
 * the caller; this module renders them as given. */
typedef struct
{
  gint corner_radius;
  gint margin;
} LcStyle;

/* Renders one full CSS rule block:
 *
 *   <selector> {
 *     background-color: <css rgba()>;
 *     border-radius: <style->corner_radius>px;
 *     margin: <style->margin>px;
 *   }
 *
 * followed by a trailing newline. selector, color and style must all be
 * non-NULL. Caller owns the returned string (g_free). */
gchar *lc_css_rule_new (const gchar *selector, const LcColor *color, const LcStyle *style);

/* Concatenates already-rendered rule fragments (e.g. from lc_css_rule_new())
 * into one CSS text fragment, in the given order, with no added banner or
 * markers. n may be 0, producing an empty string.
 * Caller owns the returned string (g_free). rules may be NULL only when
 * n == 0. */
gchar *lc_css_render_rules (const gchar *const *rules, gsize n);

G_END_DECLS

#endif /* LC_CSS_H */
