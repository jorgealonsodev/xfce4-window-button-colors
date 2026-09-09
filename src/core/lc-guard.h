/* lc-guard.h — pure xfce4-panel-only process guard (design.md D2, RNF-6)
 *
 * Links glib only. Must never include <gtk/gtk.h> or any panel/xfconf header.
 */
#ifndef LC_GUARD_H
#define LC_GUARD_H

#include <glib.h>

G_BEGIN_DECLS

/* Returns TRUE only when prgname is exactly "xfce4-panel" (case-sensitive).
 * Returns FALSE for NULL, the empty string, and every other process name,
 * including near-misses such as "xfce4-panel-wrapper" or "Xfce4-Panel".
 *
 * This is the first call in gtk_module_init(): when it returns FALSE, the
 * caller MUST return immediately without registering a CSS provider,
 * an emission hook, or any other side effect. */
gboolean lc_guard_should_activate (const gchar *prgname);

G_END_DECLS

#endif /* LC_GUARD_H */
