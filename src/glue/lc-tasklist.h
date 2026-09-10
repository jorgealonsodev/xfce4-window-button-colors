/* lc-tasklist.h — tasklist window-button detection, XID resolution, and
 * CSS class tagging.
 *
 * This is the sole owner of:
 *  - what counts as "a tasklist window button" (an XfceArrowButton whose
 *    parent is an XfceTasklist, excluding the tasklist's own overflow
 *    arrow, named "panel-tasklist-arrow");
 *  - resolving one such button to the WnckWindow it represents;
 *  - applying/removing the "lc-win-<xid>" CSS class on a button, always
 *    stripping any previous lc-win-* class first so a button GTK/XFCE
 *    recycles for a different window never keeps painting the old one.
 *
 * LcTasklistContext is the single small piece of shared, process-lifetime
 * state (the in-memory colour store, its on-disk path, and the screen the
 * CSS provider is attached to) that src/glue/module.c creates once at
 * gtk_module_init() and that lc-menu.c/lc-dialogs.c also receive, so a
 * colour change made from either of those can be applied and persisted
 * through one function (lc_tasklist_apply_change()) without each glue
 * file reaching into module.c's internals.
 *
 * Links glib, gio, gtk3 and libwnck.
 */
#ifndef LC_TASKLIST_H
#define LC_TASKLIST_H

#include <gtk/gtk.h>
#define WNCK_I_KNOW_THIS_IS_UNSTABLE
#include <libwnck/libwnck.h>

#include "lc-winstore.h"

G_BEGIN_DECLS

typedef struct _LcTasklistContext LcTasklistContext;

/* Returns the process's one WnckScreen, lazily creating the one
 * WnckHandle this module ever creates (WNCK_CLIENT_TYPE_PAGER, matching
 * a taskbar-like consumer) on first call. Centralizing this here is what
 * keeps the now-deprecated wnck_screen_get_default() out of every call
 * site that needs a screen (lc-tasklist.c itself and module.c's live-XID
 * snapshot for reconciliation). May return NULL if no display/screen is
 * available at all.
 *
 * MUST NEVER be called from gtk_module_init() or anything it calls
 * synchronously (directly proven in production: libwnck's own
 * _wnck_error_trap_push() asserts on a NULL GdkDisplay and abort()s the
 * whole panel process — not a recoverable GError). gtk_module_init() runs
 * DURING gtk_init(), before a GdkDisplay is guaranteed to exist. Every
 * call site of this function (and of lc_tasklist_resolve_window(), which
 * calls it) MUST be reachable only from something that itself cannot run
 * before a display exists: a "map"/"show" signal on a real widget, or a
 * user-triggered menu/dialog callback. Never from module.c's init path
 * directly — see module.c's own file comment for where its one
 * unavoidable live-XID snapshot is deferred to instead. */
WnckScreen *lc_tasklist_get_wnck_screen (void);

/* Creates the shared controller state. `store` and `path` are both
 * taken over by the returned context (path is copied; store is NOT
 * copied, ownership transfers to the context, which frees it in
 * lc_tasklist_context_free()). `screen` is borrowed (GDK owns it) and
 * may be NULL — see lc_tasklist_context_set_screen() below, which
 * module.c uses to fill it in once one can safely be resolved.
 * store must be non-NULL. */
LcTasklistContext *lc_tasklist_context_new (LcWinStore *store, const gchar *path, GdkScreen *screen);

/* Sets (or replaces) the screen ctx uses for lc_provider_attach() inside
 * lc_tasklist_apply_change(). Exists so module.c can construct the
 * context during gtk_module_init() — before any GdkDisplay is guaranteed
 * to exist — and only call gdk_screen_get_default() later, from a
 * deferred (post-display) callback. `screen` is borrowed. ctx must be
 * non-NULL. */
void lc_tasklist_context_set_screen (LcTasklistContext *ctx, GdkScreen *screen);

/* Frees ctx, including the store it owns. Does NOT uninstall any
 * emission hook — callers must do that first via
 * lc_tasklist_uninstall_hook()/lc_menu_uninstall_hook(). Safe with NULL. */
void lc_tasklist_context_free (LcTasklistContext *ctx);

/* Borrowed accessor to the store ctx owns, for callers (lc-menu.c,
 * lc-dialogs.c) that need to read/mutate it directly before calling
 * lc_tasklist_apply_change(). ctx must be non-NULL. */
LcWinStore *lc_tasklist_context_store (LcTasklistContext *ctx);

/* TRUE iff `widget` is a tasklist window button: an XfceArrowButton whose
 * parent is an XfceTasklist and whose name is not "panel-tasklist-arrow"
 * (the tasklist's own overflow/scroll arrow, which is the same GType but
 * is never a window). Safe with NULL/non-widget input. */
gboolean lc_tasklist_is_window_button (GtkWidget *widget);

/* Resolves `button` (must satisfy lc_tasklist_is_window_button()) to the
 * live WnckWindow it currently represents, or NULL if it cannot be
 * resolved with confidence.
 *
 * Resolution is by tooltip text (the button's tooltip is set by the
 * tasklist to the window's title, same as the window's WM name) — the
 * only signal available without linking a private libxfce4panel struct.
 * A single global title match (spike's approach) silently mispaints
 * whichever window happened to come first when two windows share a
 * title, so this instead ranks `button` among ITS OWN tasklist siblings
 * that carry the identical tooltip text (in container child order) and
 * pairs that rank with the same rank among the live WnckWindows that
 * share that title (in wnck_screen_get_windows() order). Both orderings
 * are stable within one call, so the pairing is deterministic; if the
 * rank has no corresponding live window (e.g. the tasklist currently
 * shows more same-titled buttons than wnck currently reports — a
 * transient mismatch, not a design case, since both are read in the same
 * call), this returns NULL rather than guessing by falling back to the
 * first match. */
WnckWindow *lc_tasklist_resolve_window (GtkWidget *button);

/* Re-derives `button`'s "lc-win-*" class from `store`: strips any
 * existing lc-win-* class from its style context first (recycled-button
 * safety), then resolves the window and adds the class back only if
 * store holds a colour for its xid. No-op if button is NULL or is not a
 * window button. */
void lc_tasklist_retag_button (GtkWidget *button, LcWinStore *store);

/* Installs the "map" emission hook on GtkWidget that retags every
 * tasklist window button as it maps (this is what makes stored colours
 * reappear across a panel restart, and what colours a fresh button the
 * moment it appears — for a window with no stored colour this is a
 * cheap no-op). Returns the emission-hook id for
 * lc_tasklist_uninstall_hook(). ctx must be non-NULL and stay alive for
 * as long as the hook is installed. */
gulong lc_tasklist_install_hook (LcTasklistContext *ctx);

/* Removes the emission hook installed by lc_tasklist_install_hook().
 * No-op if hook_id is 0. */
void lc_tasklist_uninstall_hook (gulong hook_id);

/* Called by lc-menu.c/lc-dialogs.c after mutating ctx's store (a
 * set/unset triggered by the user): re-renders the CSS document via
 * lc_winstore_render(), reattaches it via lc_provider_attach(), persists
 * the store via lc_winstore_save() (a save failure is only ever one
 * g_warning(), never propagated further), and retags `button`
 * immediately so the change is visible without waiting for a fresh
 * "map". `button` may be NULL (e.g. its window closed while an async
 * dialog was open) — the CSS/persistence steps still run, only the
 * immediate retag is skipped. Always persists: every caller of this
 * function has just made a real, intentional change (a user picked or
 * cleared a colour, or a window genuinely closed) — see
 * lc_tasklist_render_and_attach() below for the startup case where
 * NOTHING has changed and persisting would be wrong. ctx must be
 * non-NULL. */
void lc_tasklist_apply_change (LcTasklistContext *ctx, GtkWidget *button);

/* Renders ctx's store EXACTLY as it currently stands (no reconciliation,
 * no mutation) and attaches the CSS provider. Deliberately does NOT call
 * lc_winstore_save(): nothing changed, so there is nothing to persist —
 * a session that changes nothing must never rewrite the user's file or
 * touch its mtime. This is the ONLY startup step: no snapshot-based
 * pruning happens here or anywhere else at startup (see this header's
 * lc_tasklist_install_window_closed_hook() for why, and for the actual
 * pruning mechanism). ctx must be non-NULL and should already have a
 * screen set via lc_tasklist_context_set_screen(). */
void lc_tasklist_render_and_attach (LcTasklistContext *ctx);

/* Connects to WnckScreen::window-closed — the ONLY pruning trigger this
 * module uses. When a specific window closes, if ctx's store holds a
 * colour for its xid, that entry is removed and lc_tasklist_apply_change()
 * persists the change (a real drop happened, so saving is correct here,
 * unlike the startup path above).
 *
 * This is deliberately event-driven, never snapshot-based: a startup or
 * idle-time snapshot cannot distinguish "this window is gone" from
 * "libwnck has not enumerated it yet" — libwnck populates its window
 * list ASYNCHRONOUSLY, measured on a live panel as 0 windows at the
 * moment gtk_module_init()'s old deferred-activation snapshot ran and 29
 * windows 1.5 seconds later. Treating that ambiguity as "gone" silently
 * deleted a real user's colours for windows that were still open — see
 * lc_winstore_reconcile()'s own doc comment for the full incident. A
 * "window-closed" signal has no such ambiguity: it can only ever fire
 * for a window libwnck previously knew about and has now positively
 * confirmed gone.
 *
 * Must be called from a point where a WnckScreen can safely be resolved
 * (NOT from gtk_module_init() itself — see lc_tasklist_get_wnck_screen()).
 * Returns the signal handler id for lc_tasklist_uninstall_window_closed_hook(),
 * or 0 if no WnckScreen was available. ctx must be non-NULL and stay
 * alive for as long as the hook is installed. */
gulong lc_tasklist_install_window_closed_hook (LcTasklistContext *ctx);

/* Disconnects the handler installed by lc_tasklist_install_window_closed_hook().
 * No-op if handler_id is 0 or no WnckScreen is available. */
void lc_tasklist_uninstall_window_closed_hook (gulong handler_id);

G_END_DECLS

#endif /* LC_TASKLIST_H */
