/* lc-paths.h — XDG config path resolution (module-lifecycle spec, step 3).
 * Links glib only.
 *
 * These functions read the "HOME" environment variable directly via
 * g_getenv(), rather than relying on GLib's internally-cached
 * g_get_home_dir()/g_get_user_config_dir(), so that a missing $HOME is
 * observable and testable on every call rather than only on the very
 * first call made by the process.
 */
#ifndef LC_PATHS_H
#define LC_PATHS_H

#include <glib.h>

G_BEGIN_DECLS

/* Returns a newly allocated path to "$HOME/.config/xfce4-launcher-colors".
 * Returns NULL and emits exactly one g_warning() when $HOME is unset or
 * empty. Caller owns the returned string (g_free). */
gchar *lc_paths_config_dir (void);

/* Returns a newly allocated path to
 * "$HOME/.config/xfce4-launcher-colors/colors.css".
 * Returns NULL (via lc_paths_config_dir(), one g_warning) when $HOME is
 * unset or empty. Caller owns the returned string (g_free). */
gchar *lc_paths_colors_css (void);

/* Returns a newly allocated path to
 * "$HOME/.config/xfce4-launcher-colors/settings.ini".
 * Returns NULL (via lc_paths_config_dir(), one g_warning) when $HOME is
 * unset or empty. Caller owns the returned string (g_free). */
gchar *lc_paths_settings_ini (void);

G_END_DECLS

#endif /* LC_PATHS_H */
