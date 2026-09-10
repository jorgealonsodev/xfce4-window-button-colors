/* lc-menu.h — the per-window-button "Background colour" submenu.
 *
 * Installs a "show" emission hook (NOT "map" — see lc-menu.c's header
 * comment for the measured reason) that appends one "Background colour"
 * item, with its full quick-palette submenu, to the right-click menu of
 * every tasklist window button. Behaviour is user-approved and ported
 * from spike/tasklist-spike.c without redesign: "No colour" first and
 * always present, a separator, eight swatches with 16x16 colour images,
 * a separator, then "Custom…". A tick marks whichever entry is currently
 * applied, including "No colour" when none is.
 *
 * Links glib, gtk3 and libwnck (via lc-tasklist.h).
 */
#ifndef LC_MENU_H
#define LC_MENU_H

#include <gtk/gtk.h>
#include "lc-tasklist.h"

G_BEGIN_DECLS

/* Installs the "show" emission hook. Returns its id for
 * lc_menu_uninstall_hook(). ctx must be non-NULL and stay alive for as
 * long as the hook is installed. */
gulong lc_menu_install_hook (LcTasklistContext *ctx);

/* Removes the emission hook installed by lc_menu_install_hook().
 * No-op if hook_id is 0. */
void lc_menu_uninstall_hook (gulong hook_id);

G_END_DECLS

#endif /* LC_MENU_H */
