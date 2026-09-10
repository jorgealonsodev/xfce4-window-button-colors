/* module.c — GTK3 module entry point (design.md "Module Lifecycle and
 * Failure Paths", tasks.md 3.3/3.5).
 *
 * `gtk_module_init()`/`gtk_module_exit()` are the two symbols GTK looks
 * up by name when this .so is loaded via GTK_MODULES/`/Gtk/Modules`
 * (module-lifecycle spec). Everything here follows the lifecycle table:
 *
 *   1. guard first (design.md D2, RNF-6) — g_get_prgname() must equal
 *      "xfce4-panel" or this function returns immediately, before doing
 *      anything else: no allocation, no store load, no provider.
 *   2. static `initialized` flag — a second gtk_module_init() call, or
 *      any gtk_module_exit() call before a successful init, is a no-op.
 *   3. resolve XDG paths — a NULL $HOME degrades to "no colours": one
 *      g_warning (already emitted inside lc_paths_colors_css()) and
 *      return.
 *   4. lc_settings_load() (stubbed for this slice — see below) MUST run
 *      before any render, because the style is baked into every rule.
 *   5. lc_store_load() — never fails outward; a missing/unreadable/
 *      malformed colors.css degrades to an empty LcStore plus, at most,
 *      one warning already emitted inside lc_store_load() itself.
 *   6/6b. xfconf reconciliation (D12/D14/D15) is OUT OF SCOPE for this
 *      slice (slice 5) — the store is rendered exactly as loaded.
 *   7. lc_store_render() → lc_provider_attach() (which calls
 *      gtk_css_provider_load_from_data() internally and never
 *      *_load_from_file(), design.md D8/D16). A CSS parse failure is
 *      handled entirely inside lc_provider_attach() and never reaches
 *      here.
 *   8/8b. write-back (RF-8) is OUT OF SCOPE for this slice (slice 4/5):
 *      this slice never writes colors.css, only reads it.
 *
 * Links glib, gio and gtk3. GTK enters the tree only here and in
 * lc-provider.c — src/core/ stays glib/gio-only (design.md D1).
 */
#include <gmodule.h>
#include <gtk/gtk.h>

#include "lc-css.h"
#include "lc-document.h"
#include "lc-guard.h"
#include "lc-paths.h"
#include "lc-provider.h"
#include "lc-store.h"

G_MODULE_EXPORT void gtk_module_init (gint *argc, gchar ***argv);
G_MODULE_EXPORT void gtk_module_exit (void);

/* Step 2: idempotency. gtk_module_init() only does real work once per
 * process; gtk_module_exit() only tears down once a successful init has
 * actually run. */
static gboolean lc_module_initialized = FALSE;

/* Step 4 stub (tasks.md 3.3): the real lc-settings.ini parser is slice 5c.
 * Until then this returns the same built-in defaults the lifecycle table
 * names for a missing/unreadable settings file (radius 6, margin 2), so
 * every rule this slice renders already carries the final baked-in style
 * shape and slice 5c only needs to replace this function's body. */
static LcStyle
lc_settings_load (void)
{
  LcStyle style;

  style.corner_radius = 6;
  style.margin = 2;

  return style;
}

G_MODULE_EXPORT void
gtk_module_init (gint *argc, gchar ***argv)
{
  gchar *colors_path;
  LcStyle style;
  LcStore *store;
  LcDocument *doc;

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

  /* Step 3: a NULL $HOME already produced its one g_warning inside
   * lc_paths_colors_css(); degrade to "no colours" and stop here rather
   * than trying to load or render anything. */
  colors_path = lc_paths_colors_css ();
  if (colors_path == NULL)
    return;

  /* Step 4: settings must precede any render. */
  style = lc_settings_load ();

  /* Step 5: never fails outward (see the file comment above). */
  store = lc_store_load (colors_path);
  g_free (colors_path);

  /* Steps 6/6b (reconciliation) intentionally skipped — out of scope,
   * see the file comment above. */

  /* Step 7. */
  doc = lc_store_render (store, &style);
  lc_provider_attach (gdk_screen_get_default (), doc);

  lc_document_free (doc);
  lc_store_free (store);
}

G_MODULE_EXPORT void
gtk_module_exit (void)
{
  if (!lc_module_initialized)
    return;

  lc_provider_detach ();

  lc_module_initialized = FALSE;
}
