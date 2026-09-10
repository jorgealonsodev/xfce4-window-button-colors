/* lc-autoload-xfconf.h — the sole LcAutoloadBackend implementation
 * (src/core/lc-autoload.h), backed by the "xsettings" xfconf channel's
 * `/Gtk/Modules` property (design.md D1). This is the only file in the
 * project that reads or writes that key.
 *
 * The channel and key literals live here, not duplicated at each call
 * site, because Phase 5's lc-settings-ui.c constructs the backend
 * struct against them directly:
 *
 *   static const LcAutoloadBackend backend = {
 *     lc_autoload_xfconf_read, lc_autoload_xfconf_write
 *   };
 *   XfconfChannel *channel = xfconf_channel_get (LC_AUTOLOAD_XFCONF_CHANNEL);
 *   lc_autoload_set_enabled (&backend, channel, LC_AUTOLOAD_MODULE_NAME, enabled);
 *
 * Links xfconf, glib and gio. Never included from src/core/.
 */
#ifndef LC_AUTOLOAD_XFCONF_H
#define LC_AUTOLOAD_XFCONF_H

#include "lc-autoload.h"
#include <xfconf/xfconf.h>

G_BEGIN_DECLS

/* The channel and property this backend reads and writes (D1). */
#define LC_AUTOLOAD_XFCONF_CHANNEL "xsettings"
#define LC_AUTOLOAD_XFCONF_KEY     "/Gtk/Modules"

/* The module name lc_autoload_set_enabled()'s `name` argument is called
 * with — the .so basename meson.build's shared_module() target installs
 * (matches gettext_package / meson.project_name(), see meson.build). */
#define LC_AUTOLOAD_MODULE_NAME "xfce4-window-button-colors"

/* LcAutoloadBackend::read. `user_data` MUST be a live XfconfChannel* for
 * LC_AUTOLOAD_XFCONF_CHANNEL (e.g. from xfconf_channel_get()). See
 * lc-autoload.h for the full read contract this must honour: report
 * ABSENT/SCALAR/ARRAY/UNKNOWN exactly as found, and for SCALAR/ARRAY
 * hand back an ALREADY NORMALIZED list (lc_autoload_normalize() is
 * called internally — the caller must not normalize again). */
gboolean lc_autoload_xfconf_read  (gpointer user_data, LcAutoloadShape *out_shape, gchar ***out_modules);

/* LcAutoloadBackend::write. `user_data` MUST be a live XfconfChannel*
 * for LC_AUTOLOAD_XFCONF_CHANNEL. `shape` is always one of ABSENT
 * (delete the key, `modules` ignored), SCALAR or ARRAY — never UNKNOWN;
 * lc_autoload_set_enabled() never calls write() with UNKNOWN (D1). */
gboolean lc_autoload_xfconf_write (gpointer user_data, LcAutoloadShape shape, const gchar *const *modules);

G_END_DECLS

#endif /* LC_AUTOLOAD_XFCONF_H */
