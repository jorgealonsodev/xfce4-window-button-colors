/* module.c — GTK3 module entry point.
 *
 * `gtk_module_init()`/`gtk_module_exit()` are the two symbols GTK looks
 * up by name when this .so is loaded via GTK_MODULES/`/Gtk/Modules`.
 *
 * *** Three production incidents fixed here — READ BEFORE TOUCHING ***
 *
 * 1. Crash-on-load (RNF-6 violation). gtk_module_init() runs DURING
 *    gtk_init(), before a GdkDisplay is guaranteed to exist. A libwnck
 *    call reached synchronously from here made libwnck's own internal
 *    _wnck_error_trap_push() assert on a NULL GdkDisplay and abort() the
 *    whole panel process — not a recoverable GError, a hard abort, on a
 *    live xfce4-panel 4.18.4. Fix: gtk_module_init() itself now does
 *    ONLY display-independent work (guard, idempotency, gettext
 *    binding, path resolution, lc_winstore_load(), installing the
 *    "map"/"show" emission hooks). Everything display/wnck-dependent is
 *    deferred to lc_module_deferred_activate() below, scheduled via
 *    g_idle_add() so it runs once the main loop is actually iterating.
 *
 * 2. libwnck reentrancy CRITICAL. wnck_screen_force_update() was called
 *    from the deferred step. The panel's own tasklist plugin is itself a
 *    libwnck client already inside update_client_list() at that point in
 *    startup, so force_update() re-entered it: "Wnck-CRITICAL **:
 *    update_client_list: assertion 'reentrancy_guard == 0' failed",
 *    confirmed on a live panel, and fatal under G_DEBUG=fatal-criticals.
 *    Fix: force_update() is never called anywhere in this module.
 *
 * 3. Silent data loss (the most serious). With (1) and (2) fixed, the
 *    deferred step still reconciled the freshly-loaded store against a
 *    wnck snapshot taken at g_idle_add time. Measured on a live panel:
 *    libwnck populates its window list ASYNCHRONOUSLY — 0 windows at
 *    that exact moment, 29 windows 1.5 seconds later. The old
 *    lc_winstore_reconcile() policy treated "successfully obtained, zero
 *    windows" as positive proof every stored colour's window was gone,
 *    and deleted three real, live colours with no warning. Fix, two
 *    parts:
 *      a. lc_winstore_reconcile() itself now treats an empty-but-obtained
 *         snapshot exactly like an unobtainable one — keep everything,
 *         prune nothing (see its own doc comment in lc-winstore.h).
 *      b. This module does not call lc_winstore_reconcile() from any
 *         startup/idle snapshot AT ALL, even with that safer policy: a
 *         non-empty snapshot taken too early can still be a PARTIAL one,
 *         which n_live alone cannot detect. Pruning instead happens only
 *         on positive per-window evidence, via lc_tasklist_install_window_closed_hook()
 *         (WnckScreen::window-closed) — a signal that, unlike any
 *         snapshot, cannot fire for a window that merely has not been
 *         enumerated yet. The deferred step below therefore only
 *         installs that hook and renders/attaches whatever was loaded
 *         from disk, unmodified — see lc_tasklist_render_and_attach()'s
 *         own doc comment for why that step must not save either
 *         ("never rewrite the file when nothing changed").
 *
 * Lifecycle:
 *   1. guard first — g_get_prgname() must equal "xfce4-panel" or this
 *      function returns immediately, before doing anything else.
 *   2. idempotency — a second gtk_module_init() call, or any
 *      gtk_module_exit() call before a successful init, is a no-op.
 *   3. i18n setup — bind the text domain so every _() call downstream
 *      resolves.
 *   4. resolve the persistence path — a NULL $HOME degrades to
 *      "no colours" and this returns.
 *   5. lc_winstore_load() — never fails outward.
 *   6. install the tasklist "map" hook and the menu "show" hook against
 *      an LcTasklistContext that owns the store but has NO screen yet.
 *   7. schedule lc_module_deferred_activate() via g_idle_add(): once the
 *      main loop runs and a display exists, it resolves the screen,
 *      installs the "window-closed" prune hook, and renders+attaches
 *      the store exactly as loaded — no reconciliation, no save.
 *
 * Links glib, gio, gtk3 and libwnck. GTK enters the tree only here and
 * in lc-provider.c/lc-tasklist.c/lc-menu.c/lc-dialogs.c — src/core/
 * stays glib/gio-only.
 */
#include <gmodule.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "lc-guard.h"
#include "lc-menu.h"
#include "lc-paths.h"
#include "lc-provider.h"
#include "lc-tasklist.h"
#include "lc-winstore.h"

G_MODULE_EXPORT void gtk_module_init (gint *argc, gchar ***argv);
G_MODULE_EXPORT void gtk_module_exit (void);

/* Step 2: idempotency. */
static gboolean lc_module_initialized = FALSE;

/* Process-lifetime state, populated by a successful gtk_module_init()
 * and torn down by the matching gtk_module_exit(). */
static LcTasklistContext *lc_module_ctx = NULL;
static gulong lc_module_tasklist_hook_id = 0;
static gulong lc_module_menu_hook_id = 0;
static gulong lc_module_window_closed_hook_id = 0;
static guint lc_module_deferred_source_id = 0;

/* Runs exactly once (g_idle_add() one-shot: returns G_SOURCE_REMOVE),
 * the first time the main loop actually iterates — see this file's
 * header comment for exactly why this, and not gtk_module_init() itself,
 * is where the display-dependent half of startup belongs, and why it
 * performs NO reconciliation. */
static gboolean
lc_module_deferred_activate (gpointer user_data)
{
  LcTasklistContext *ctx = user_data;

  lc_module_deferred_source_id = 0;

  lc_tasklist_context_set_screen (ctx, gdk_screen_get_default ());

  /* The ONLY pruning trigger this module uses. No snapshot-based prune
   * happens here or anywhere else at startup — see this file's header
   * comment, incident 3. */
  lc_module_window_closed_hook_id = lc_tasklist_install_window_closed_hook (ctx);

  /* Renders and attaches exactly what was loaded from disk. Nothing was
   * reconciled, so there is nothing to persist either. */
  lc_tasklist_render_and_attach (ctx);

  return G_SOURCE_REMOVE;
}

G_MODULE_EXPORT void
gtk_module_init (gint *argc, gchar ***argv)
{
  gchar *colors_path;
  LcWinStore *store;

  (void) argc;
  (void) argv;

  /* Step 1: guard first, before the idempotency check or anything else.
   * In any process that is not xfce4-panel this is the entire cost: one
   * string compare, nothing allocated, no hook, no provider. */
  if (!lc_guard_should_activate (g_get_prgname ()))
    return;

  /* Step 2. */
  if (lc_module_initialized)
    return;
  lc_module_initialized = TRUE;

  /* Step 3. */
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  /* Step 4: a NULL $HOME already produced its one g_warning inside
   * lc_paths_colors_css(); degrade to "no colours" and stop here rather
   * than trying to load or render anything. */
  colors_path = lc_paths_colors_css ();
  if (colors_path == NULL)
    return;

  /* Step 5: never fails outward (see lc-winstore.h). */
  store = lc_winstore_load (colors_path);

  /* Step 6: the context is created now (entirely display-independent)
   * but carries NO screen yet — lc_module_deferred_activate() fills that
   * in once one can safely be resolved. */
  lc_module_ctx = lc_tasklist_context_new (store, colors_path, NULL);
  g_free (colors_path);

  lc_module_tasklist_hook_id = lc_tasklist_install_hook (lc_module_ctx);
  lc_module_menu_hook_id = lc_menu_install_hook (lc_module_ctx);

  /* Step 7. */
  lc_module_deferred_source_id = g_idle_add (lc_module_deferred_activate, lc_module_ctx);
}

G_MODULE_EXPORT void
gtk_module_exit (void)
{
  if (!lc_module_initialized)
    return;

  /* If the deferred activation never got to run (module unloaded almost
   * immediately), cancel it rather than let it fire later against a
   * context that is about to be freed below. */
  if (lc_module_deferred_source_id != 0)
    {
      g_source_remove (lc_module_deferred_source_id);
      lc_module_deferred_source_id = 0;
    }

  lc_tasklist_uninstall_window_closed_hook (lc_module_window_closed_hook_id);
  lc_module_window_closed_hook_id = 0;

  lc_menu_uninstall_hook (lc_module_menu_hook_id);
  lc_tasklist_uninstall_hook (lc_module_tasklist_hook_id);
  lc_module_menu_hook_id = 0;
  lc_module_tasklist_hook_id = 0;

  lc_provider_detach ();

  lc_tasklist_context_free (lc_module_ctx);
  lc_module_ctx = NULL;

  lc_module_initialized = FALSE;
}
