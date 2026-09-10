/* lc-dialogs.h — the full-range colour dialog and screen eyedropper.
 *
 * Entirely async: no gtk_dialog_run() and no gtk_main_iteration() wait
 * loop anywhere in this file. A nested main loop running inside the
 * panel process can act on already-destroyed widgets and wedge the
 * user's desktop, which is exactly the spike shortcut this production
 * module must not carry over (spike/tasklist-spike.c's own header
 * comment on pick_from_screen() flags this explicitly).
 *
 * Links glib, gtk3 and libwnck (via lc-tasklist.h).
 */
#ifndef LC_DIALOGS_H
#define LC_DIALOGS_H

#include <gtk/gtk.h>
#include "lc-tasklist.h"

G_BEGIN_DECLS

/* Opens the full-range colour chooser dialog for the window `button`
 * resolves to (lc_tasklist_resolve_window()). If it cannot be resolved,
 * this emits one g_warning() and returns without showing anything.
 *
 * The dialog: alpha on, seeded with the window's current colour (if
 * any), a "No colour" button (GTK_RESPONSE_REJECT), an eyedropper icon
 * button that starts an async screen-colour pick without closing the
 * dialog, auxiliary buttons (No colour, eyedropper) in the secondary
 * button-box group, and the confirming OK button last, carrying a live
 * preview of the colour being chosen with its label switched black/white
 * by relative luminance.
 *
 * Shown via gtk_widget_show() plus a "response" handler — never
 * gtk_dialog_run(). ctx and button must both be non-NULL. */
void lc_dialogs_open_for_button (LcTasklistContext *ctx, GtkWidget *button);

G_END_DECLS

#endif /* LC_DIALOGS_H */
