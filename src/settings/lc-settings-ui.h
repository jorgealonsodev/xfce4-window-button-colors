/* lc-settings-ui.h — the toggle row and the restart action
 * (design.md D1/D4/D5, tasks.md Phase 5).
 *
 * The xfconf write (via lc_autoload_set_enabled()) and the panel
 * restart (via lc_restart_panel_argv() + g_spawn_async()) are two
 * separate, explicit, user-triggered actions — the toggle NEVER
 * restarts the panel itself (module-autoload-toggle "Toggle completes
 * without restarting"). Restarting always goes through a cancellable
 * confirmation dialog that names the TTY recovery from README.md:113,
 * so a user about to take the risky action can read how to undo it in
 * the same breath.
 *
 * Links gtk, xfconf and glib — src/settings/ is where all three are
 * allowed (design.md "Technical Approach").
 */
#ifndef LC_SETTINGS_UI_H
#define LC_SETTINGS_UI_H

#include <gtk/gtk.h>
#include <xfconf/xfconf.h>

G_BEGIN_DECLS

/* Builds the toggle row + restart action box for the settings window.
 * `channel` must be a live XfconfChannel* for the "xsettings" channel
 * (LC_AUTOLOAD_XFCONF_CHANNEL, lc-autoload-xfconf.h), already obtained
 * via xfconf_channel_get(). This widget does not take ownership of
 * `channel` — the caller (main.c) keeps it alive for at least as long
 * as the returned widget; the widget disconnects its own
 * "property-changed" handler when it is destroyed (D5), so `channel`
 * may safely outlive it.
 *
 * On creation, the toggle immediately reflects the module's current
 * enabled state (lc_autoload_is_enabled()) read from `channel`.
 *
 * Returns a new GtkWidget ready to be packed into a container. Never
 * NULL. */
GtkWidget *lc_settings_ui_new (XfconfChannel *channel);

G_END_DECLS

#endif /* LC_SETTINGS_UI_H */
