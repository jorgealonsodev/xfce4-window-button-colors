/* lc-settings-ui.h — the toggle row and the restart action
 * (design.md D1/D2/D4/D5, tasks.md Phase 5).
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
 * The stored-colour management section (the row list, per-row removal,
 * and "Clean Up Orphaned Entries") has been removed from this file.
 * "Clean Up Orphaned Entries" caused real, reproduced data loss on the
 * maintainer's own configuration — colours were deleted for windows
 * that were still open — and the defect was not reliably reproducible
 * (same fixture, same single click: once a full wipe, once correct),
 * so a root cause could not be confirmed in the settings-app glue code.
 * A button that sometimes destroys user data must not ship; the
 * section is withdrawn until the cause is found. The pure logic it
 * depended on (src/core/lc-winlist.{c,h}, src/core/lc-winstore.{c,h})
 * is untouched and proven correct in isolation — it stays for when the
 * feature returns.
 *
 * Links gtk, xfconf and glib — src/settings/ is where both are
 * allowed (design.md "Technical Approach").
 */
#ifndef LC_SETTINGS_UI_H
#define LC_SETTINGS_UI_H

#include <gtk/gtk.h>
#include <xfconf/xfconf.h>

G_BEGIN_DECLS

/* Builds the toggle row and restart action for the settings window.
 *
 * `channel` must be a live XfconfChannel* for the "xsettings" channel
 * (LC_AUTOLOAD_XFCONF_CHANNEL, lc-autoload-xfconf.h), already obtained
 * via xfconf_channel_get().
 *
 * This widget does not take ownership of `channel` — the caller
 * (main.c) keeps it alive for at least as long as the returned widget;
 * the widget disconnects its own "property-changed" handler when it is
 * destroyed (D5), so `channel` may safely outlive it.
 *
 * On creation, the toggle immediately reflects the module's current
 * enabled state (lc_autoload_is_enabled()) read from `channel`.
 *
 * Returns a new GtkWidget ready to be packed into a container. Never
 * NULL. */
GtkWidget *lc_settings_ui_new (XfconfChannel *channel);

G_END_DECLS

#endif /* LC_SETTINGS_UI_H */
