/* lc-restart.h — the panel-restart argv vector (design.md D4).
 *
 * D4's decision: restarting xfce4-panel is always a plain argv vector run
 * through g_spawn_async(), never a shell string through
 * g_spawn_command_line_async() or a "sh -c ..." wrapper. That is exactly
 * the launcher-era threat-matrix rule ("argv vector, never
 * g_spawn_command_line_*, never a shell string"), carried over here and
 * applying harder: this is a button a user clicks, not a fixed CLI
 * invocation, so it is a standing target for a future "just make the
 * command configurable" change that would otherwise be tempted to grow a
 * shell string.
 *
 * This file owns only the vector — the pure, testable half. The actual
 * g_spawn_async() call, its confirmation dialog, and its non-modal error
 * surfacing on failure all live in src/settings/ (Phase 5), never here.
 *
 * Links glib only. Must never include <gtk/gtk.h> or any panel/xfconf
 * header.
 */
#ifndef LC_RESTART_H
#define LC_RESTART_H

#include <glib.h>

G_BEGIN_DECLS

/* Returns a newly allocated argv vector for restarting xfce4-panel:
 * { "xfce4-panel", "-r", NULL }. Pure: no I/O, and it never calls
 * g_spawn_async() itself — the caller passes this straight through to
 * g_spawn_async() with G_SPAWN_SEARCH_PATH (design D4). Never returns
 * NULL. Caller owns the returned vector (g_strfreev()). */
gchar **lc_restart_panel_argv (void);

G_END_DECLS

#endif /* LC_RESTART_H */
