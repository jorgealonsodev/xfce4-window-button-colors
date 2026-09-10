/* main.c — entry point for the standalone settings application
 * (design.md "Technical Approach"): a separate GtkApplication process,
 * never loaded in-process by xfce4-panel, so it stays reachable exactly
 * in the state the user must escape (module disabled in /Gtk/Modules)
 * and its own failures can never abort the panel.
 *
 * Phase 4 built the bare shell; Phase 5 (design.md D1/D4/D5) added the
 * xfconf channel this process owns for its whole lifetime and embedded
 * lc-settings-ui.c's toggle row and restart action into the window.
 * Phase 7 (design.md D2/D7/D8) adds the LcWinStore this process loads
 * once at startup, feeding lc-settings-ui.c's colour list.
 *
 * Links gtk, gio, xfconf and glib — this is src/settings/, where GTK,
 * xfconf and libwnck are allowed to appear (design.md "Technical
 * Approach"; src/core/ must never see any of them, which CI enforces
 * by scanning liblc-core.a for undefined gtk_/xfconf_/wnck_ symbols).
 */
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <xfconf/xfconf.h>

#include "lc-autoload-xfconf.h"
#include "lc-paths.h"
#include "lc-settings-ui.h"
#include "lc-winstore.h"

/* Reverse-DNS GApplication id. Does not need to match any D-Bus service
 * this project ships today — GtkApplication requires a well-formed id
 * even for a single-instance-by-default local application. */
#define LC_SETTINGS_APPLICATION_ID "io.github.jorgealonsodev.xfce4-window-button-colors-settings"

/* Bundles everything "activate" needs beyond the GtkApplication itself.
 * Owned by main() for the whole process lifetime (stack-allocated there,
 * alive for the full g_application_run() call) — lc-settings-ui.c only
 * ever borrows `channel`/`store`/`colors_path` from this, exactly as its
 * own header documents. */
typedef struct
{
  XfconfChannel *channel;
  LcWinStore    *store;
  gchar         *colors_path; /* may be NULL — see lc_paths_colors_css() */
} LcSettingsAppContext;

static void
activate (GtkApplication *app, gpointer user_data)
{
  LcSettingsAppContext *ctx = user_data;
  GtkWidget *window;
  GtkWidget *ui;

  window = gtk_application_window_new (app);
  gtk_window_set_title (GTK_WINDOW (window), _("Window Button Colors"));
  gtk_window_set_default_size (GTK_WINDOW (window), 480, 360);

  /* lc-settings-ui.c: toggle row bound to lc_autoload_is_enabled()/
   * lc_autoload_set_enabled() via the xfconf backend, the separate
   * restart action (D1/D4/D5), and the stored-colour list joined
   * against a live wnck snapshot (D2/D7/D8). */
  ui = lc_settings_ui_new (ctx->channel, ctx->store, ctx->colors_path);
  gtk_container_add (GTK_CONTAINER (window), ui);

  gtk_widget_show_all (window);
}

int
main (int argc, char **argv)
{
  GtkApplication *app;
  LcSettingsAppContext ctx = { NULL, NULL, NULL };
  GError *error = NULL;
  int status;

  /* Gettext domain binding, the same three-call sequence
   * src/glue/module.c uses for the panel module's own translations
   * (GETTEXT_PACKAGE/LOCALEDIR come from this target's c_args in
   * meson.build, mirroring lc_glue_c_args). */
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  /* xfconf_init()/xfconf_channel_get() only ever READ /Gtk/Modules
   * until a user actually flips the toggle in lc-settings-ui.c — this
   * call alone never writes anything. One channel, obtained once, lives
   * for this process's whole run and is handed to lc-settings-ui.c. */
  if (!xfconf_init (&error))
    {
      g_printerr ("%s: %s\n", g_get_prgname (), error->message);
      g_error_free (error);
      return 1;
    }

  ctx.channel = xfconf_channel_get (LC_AUTOLOAD_XFCONF_CHANNEL);

  /* lc_paths_colors_css() returns NULL (with its own one g_warning) only
   * when $HOME is unset or empty — lc_winstore_load(NULL) degrades to a
   * genuinely empty store rather than failing outward (see lc-winstore.h),
   * so the window still opens and is usable
   * (settings-app-shell "Launch while module is disabled"/"...enabled"
   * both require the window to open regardless); ctx.colors_path simply
   * stays NULL and every colour-list mutation becomes in-memory-only for
   * this run (see lc-settings-ui.h). */
  ctx.colors_path = lc_paths_colors_css ();
  ctx.store = lc_winstore_load (ctx.colors_path);

  /* G_APPLICATION_DEFAULT_FLAGS replaces G_APPLICATION_FLAGS_NONE (same
   * value) starting glib 2.74; glib_dep's declared floor in meson.build
   * is 2.56, where the new name does not exist yet, so pick whichever
   * this build's headers actually provide instead of hardcoding one and
   * either warning under -Werror (old name, newer glib) or failing to
   * compile (new name, older glib). */
#if GLIB_CHECK_VERSION (2, 74, 0)
  app = gtk_application_new (LC_SETTINGS_APPLICATION_ID, G_APPLICATION_DEFAULT_FLAGS);
#else
  app = gtk_application_new (LC_SETTINGS_APPLICATION_ID, G_APPLICATION_FLAGS_NONE);
#endif
  g_signal_connect (app, "activate", G_CALLBACK (activate), &ctx);

  status = g_application_run (G_APPLICATION (app), argc, argv);

  g_object_unref (app);
  lc_winstore_free (ctx.store);
  g_free (ctx.colors_path);

  /* Deliberately NOT g_object_unref (ctx.channel) here: unlike
   * xfconf_channel_new(), xfconf_channel_get() (used above) is
   * G_GNUC_WARN_UNUSED_RESULT-free in xfconf-channel.h — it hands back
   * a reference into xfconf's own internal channel cache, which
   * xfconf_shutdown() below already walks and releases itself. Manually
   * unreffing it first (Phase 5's original shape here) drops the
   * channel's last reference early, and xfconf_shutdown()'s own cache
   * walk then unrefs the now-freed object — confirmed as a real
   * SIGSEGV in g_object_unref() from inside xfconf_shutdown(), caught
   * during Phase 7's mandated Xvfb clean-exit verification (task 7.3),
   * not a hypothetical. */
  xfconf_shutdown ();

  return status;
}
