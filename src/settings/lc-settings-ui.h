/* lc-settings-ui.h — the toggle row, the restart action, and the
 * stored-colour list (design.md D1/D2/D4/D5, tasks.md Phases 5 and 7).
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
 * The colour list (Phase 7) is a view over `store`, joined against a
 * fresh wnck snapshot captured by lc-winlist-wnck.c and built by the
 * pure src/core/lc-winlist.c — this file never talks to libwnck
 * directly. Refreshed on every GtkWidget::map of the list's own
 * container (design D2's "the window is realized and mapped" gate) and
 * after every store mutation this file makes (single removal, bulk
 * cleanup), so the displayed rows always match `store`'s current
 * contents.
 *
 * Links gtk, xfconf and glib — src/settings/ is where all three are
 * allowed (design.md "Technical Approach").
 */
#ifndef LC_SETTINGS_UI_H
#define LC_SETTINGS_UI_H

#include <gtk/gtk.h>
#include <xfconf/xfconf.h>

#include "lc-winstore.h"

G_BEGIN_DECLS

/* Builds the toggle row, restart action, and stored-colour list box for
 * the settings window.
 *
 * `channel` must be a live XfconfChannel* for the "xsettings" channel
 * (LC_AUTOLOAD_XFCONF_CHANNEL, lc-autoload-xfconf.h), already obtained
 * via xfconf_channel_get(). `store` must be a live LcWinStore*
 * (lc_winstore_load()), and `colors_path` — used only as the argument
 * to lc_winstore_save() after a mutation — may be NULL (e.g. $HOME was
 * unset when the caller resolved it via lc_paths_colors_css(), which
 * already emitted its own warning); a NULL `colors_path` makes every
 * mutation an in-memory-only change for this run, never a crash or a
 * spurious warning from lc_winstore_save() itself.
 *
 * This widget does not take ownership of `channel`, `store`, or
 * `colors_path` — the caller (main.c) keeps all three alive for at
 * least as long as the returned widget; the widget disconnects its own
 * "property-changed" handler when it is destroyed (D5), so `channel`
 * may safely outlive it.
 *
 * On creation, the toggle immediately reflects the module's current
 * enabled state (lc_autoload_is_enabled()) read from `channel`. The
 * colour list itself is empty until its container is first mapped —
 * building it needs a mapped toplevel window for the D2 self-XID probe
 * (see lc-winlist-wnck.h), which cannot happen before then.
 *
 * Returns a new GtkWidget ready to be packed into a container. Never
 * NULL. */
GtkWidget *lc_settings_ui_new (XfconfChannel *channel, LcWinStore *store, const gchar *colors_path);

G_END_DECLS

#endif /* LC_SETTINGS_UI_H */
