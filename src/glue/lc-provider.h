/* lc-provider.h — sole owner of the GtkCssProvider priority decision and
 * of the attach/detach lifecycle for the module's ONE CSS provider
 * (design.md D6, D7, D8, D16).
 *
 * D7: LC_PROVIDER_PRIORITY (defined at build time from the meson option
 * `provider_priority`, resolved to a GTK_STYLE_PROVIDER_PRIORITY_* literal
 * as a -D compile define) is named ONLY inside lc-provider.c. No other
 * file in the tree may name a GtkCssProvider priority constant or attach
 * a provider to a screen — src/glue/module.c calls lc_provider_attach()
 * and never touches GtkCssProvider directly.
 *
 * D16: the only input this file accepts is an opaque LcDocument *, whose
 * sole constructor is lc_store_render() (src/core/lc-store.c). There is
 * no function here that takes a path or raw CSS text, so handing this
 * file `g_file_get_contents ("colors.css")` does not compile — the exact
 * bypass D16 exists to close.
 *
 * Links glib, gio and gtk3.
 */
#ifndef LC_PROVIDER_H
#define LC_PROVIDER_H

#include <gtk/gtk.h>
#include "lc-document.h"

G_BEGIN_DECLS

/* Attaches (or reloads) the module's sole GtkCssProvider for `screen`,
 * rendering `doc`'s CSS text via gtk_css_provider_load_from_data() ONLY
 * (design.md D8) — never gtk_css_provider_load_from_file(). This is the
 * only function in the tree that may call
 * gtk_style_context_add_provider_for_screen() (design.md D6: one
 * provider, whole-document reload): a second call replaces the
 * previously attached provider rather than stacking a new one.
 *
 * If parsing doc's rendered CSS fails, the GError is caught here, exactly
 * one g_warning() is emitted, the failed rule set is discarded, and the
 * previously attached provider (if any) is left attached and unchanged —
 * the failure is never propagated to the caller (design.md lifecycle
 * step 7, RNF-6).
 *
 * doc must be non-NULL. If screen is NULL (no default screen available),
 * this is a silent, warned no-op — never a crash (RNF-6). */
void lc_provider_attach (GdkScreen *screen, const LcDocument *doc);

/* Detaches and releases the currently attached provider, if any, leaving
 * every screen it was attached to unstyled by us again. Safe to call
 * when nothing is attached and safe to call more than once (module.c's
 * gtk_module_exit() idempotency, design.md lifecycle step 2). */
void lc_provider_detach (void);

G_END_DECLS

#endif /* LC_PROVIDER_H */
